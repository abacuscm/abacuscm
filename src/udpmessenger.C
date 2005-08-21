#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "peermessenger.h"
#include "logger.h"
#include "config.h"
#include "message.h"
#include "dbcon.h"

#include <map>

using namespace std;

#define ST_FRAME_SIZE		17

#define MIN_FRAGMENT_SIZE			200
#define DEFAULT_MAX_FRAGMENT_SIZE	1000
#define BUFFER_SIZE					1408
#define MAX_FRAGMENT_SIZE			((unsigned)(BUFFER_SIZE - sizeof(st_frame) + 1))
#define MAX_BLOCKSIZE				32

struct st_frame {
	uint32_t server_id;
	uint32_t message_id;
	uint16_t fragment_num;
	uint16_t fragment_len;
	uint32_t data_checksum;
	uint8_t data[1];
} __attribute__ ((packed));

struct st_fragment {
	uint16_t fragment_length;
	uint8_t data[1];
} __attribute__ ((packed));

struct st_fragment_collection {
	uint16_t num_fragments;
	uint16_t fragments_received;
	map<uint16_t, st_fragment*> fragment;
};

class UDPPeerMessenger : public PeerMessenger {
private:
	int _sock;
	uint16_t _max_fragment_size;
	uint32_t _checksum_seed;
	const EVP_CIPHER *_cipher;
	unsigned char* _cipher_key;
	unsigned char* _cipher_iv;
	int _cipher_blocksize;
	int _cipher_ivsize;
	int _cipher_keysize;
	EVP_CIPHER_CTX _enc_ctx;
	EVP_CIPHER_CTX _dec_ctx;
	pthread_mutex_t _enc_lock;

	map<uint32_t, struct sockaddr_in> addrmap;

	const sockaddr_in* getSockAddr(uint32_t server_id);

	uint32_t checksum(const uint8_t *data, uint16_t len);

	bool startup();
public:
	UDPPeerMessenger();
	virtual ~UDPPeerMessenger();

	virtual bool initialise();
	virtual void deinitialise();
	virtual bool sendMessage(uint32_t server_id, const Message * message);
	virtual Message* getMessage();
};

static void log_ciphers(const OBJ_NAME *name, void*);

UDPPeerMessenger::UDPPeerMessenger() {
	_sock = -1;
	_max_fragment_size = 0;
	_checksum_seed = 0;
	_cipher = NULL;
	_cipher_key = NULL;
	_cipher_iv = NULL;
	_cipher_blocksize = -1;
	_cipher_ivsize = -1;
	_cipher_keysize = -1;
}

UDPPeerMessenger::~UDPPeerMessenger() {
	if(_sock > 0)
		deinitialise();
}

bool UDPPeerMessenger::initialise() {
	if(_sock < 0)
		return startup();
	return true;
}

void UDPPeerMessenger::deinitialise() {
	if(_sock > 0) {
		close(_sock);
		_sock = -1;
	}
	if(_cipher_key) {
		munmap(_cipher_key, _cipher_keysize);
		_cipher_key = NULL;
	}
	if(_cipher_iv) {
		munmap(_cipher_iv, _cipher_ivsize);
		_cipher_iv = NULL;
	}
	EVP_CIPHER_CTX_cleanup(&_enc_ctx);
	EVP_CIPHER_CTX_cleanup(&_dec_ctx);
	pthread_mutex_destroy(&_enc_lock);
}

bool UDPPeerMessenger::startup() {
	struct sockaddr_in sock_addr;
	uint16_t portnum;
	Config &config = Config::getConfig();
	string cipher_string = config["udpmessenger"]["cipher"];
	string cipher_key = config["udpmessenger"]["keyfile"];
	string cipher_iv = config["udpmessenger"]["ivfile"];
	int fd;

	_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if(_sock < 0) {
		lerror("socket");
		goto err;
	}

	portnum = atol(config["udpmessenger"]["port"].c_str());
	if(!portnum)
		portnum = DEFAULT_UDPRECEIVER_PORT;

	log(LOG_INFO, "Binding UDP Peer Messenger to port %d.", portnum);
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(portnum);
	sock_addr.sin_addr.s_addr = INADDR_ANY;

	if(bind(_sock, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) < 0) {
		lerror("bind");
		goto err;
	}
	
	_max_fragment_size = atol(config["udpmessenger"]["maxfragmentsize"].c_str());
	if(_max_fragment_size < MIN_FRAGMENT_SIZE) {
		log(LOG_ERR, "maxfragmentsize (%u) is too small, increasing to %u.", _max_fragment_size, MIN_FRAGMENT_SIZE);
		_max_fragment_size = MIN_FRAGMENT_SIZE;
	} else if(_max_fragment_size > MAX_FRAGMENT_SIZE) {
		log(LOG_ERR, "maxfragmentsize (%u) is too big, decreasing to %u.", _max_fragment_size, MAX_FRAGMENT_SIZE);
		_max_fragment_size = MAX_FRAGMENT_SIZE;
	} else
		log(LOG_INFO, "Using maxfragmentsize of %u.", _max_fragment_size);

	OpenSSL_add_all_algorithms();

	_cipher = EVP_get_cipherbyname(cipher_string.c_str());
	if(!_cipher) {
		log_ssl_errors("EVP_get_cipherbyname");
		log(LOG_ERR, "Unable to get cipher for '%s', available options are:",
				cipher_string.c_str());

		OBJ_NAME_do_all_sorted(OBJ_NAME_TYPE_CIPHER_METH,
				log_ciphers, NULL);
		
		goto err;
	}

	_cipher_keysize = EVP_CIPHER_key_length(_cipher);
	_cipher_blocksize = EVP_CIPHER_block_size(_cipher);
	_cipher_ivsize = EVP_CIPHER_iv_length(_cipher);

	if(_cipher_blocksize > MAX_BLOCKSIZE) {
		log(LOG_ERR, "Cipher '%s' requires a blocksize of %d which is "
				"greater than the compiled-in max of %d", cipher_string.c_str(),
				_cipher_blocksize, MAX_BLOCKSIZE);
		goto err;
	}

	log(LOG_INFO, "Using cipher %s with blocksize %d, keysize %d "
			"and iv length %d.", cipher_string.c_str(),
			_cipher_blocksize, _cipher_keysize, _cipher_ivsize);

	fd = open(cipher_key.c_str(), O_RDONLY);
	if(fd < 0) {
		lerror("open");
		goto err;
	}

	_cipher_key = (unsigned char*)mmap(NULL, _cipher_keysize, PROT_READ, MAP_PRIVATE, fd, 0);
	if(_cipher_key == MAP_FAILED) {
		lerror("mmap");
		goto err;
	}

	close(fd);
	
	fd = open(cipher_iv.c_str(), O_RDONLY);
	if(fd < 0) {
		lerror("open");
		goto err;
	}

	_cipher_iv = (unsigned char*)mmap(NULL, _cipher_ivsize, PROT_READ, MAP_PRIVATE, fd, 0);
	if(_cipher_iv == MAP_FAILED) {
		lerror("mmap");
		goto err;
	}

	close(fd);

	// There were used to debug the code above but can be a security risk
	// if we are logging with syslog() and using remote logging.
//	log_buffer(LOG_DEBUG, "cipher key", _cipher_key, _cipher_keysize);
//	log_buffer(LOG_DEBUG, "cipher iv", _cipher_iv, _cipher_ivsize);
	
	EVP_CIPHER_CTX_init(&_enc_ctx);
	EVP_EncryptInit(&_enc_ctx, _cipher, _cipher_key, _cipher_iv);
	
	EVP_CIPHER_CTX_init(&_dec_ctx);
	EVP_DecryptInit(&_dec_ctx, _cipher, _cipher_key, _cipher_iv);

	pthread_mutex_init(&_enc_lock, NULL);

	log(LOG_INFO, "UDPPeerMessenger started up");
	return true;

err:
	if(_sock > 0) {
		close(_sock);
		_sock = -1;
	}
	if(_cipher_key) {
		munmap(_cipher_key, _cipher_keysize);
		_cipher_key = NULL;
	}
	if(_cipher_iv) {
		munmap(_cipher_iv, _cipher_ivsize);
		_cipher_iv = NULL;
	}
	return false;
}

uint32_t UDPPeerMessenger::checksum(const uint8_t *data, uint16_t len) {
	uint32_t checksum = _checksum_seed;

	while(len >= sizeof(uint32_t)) {
		checksum ^= *(uint32_t*)data;
		data += sizeof(uint32_t);
		len -= sizeof(uint32_t);
	}

	uint32_t last = 0;
	while(len) {
		last = last << 8 | *data;
		++data;
		--len;
	}

	checksum ^= last;
	return checksum;
}

const sockaddr_in* UDPPeerMessenger::getSockAddr(uint32_t server_id) {
	static map<uint32_t, struct sockaddr_in> cache;
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

	sockaddr_in *result = NULL;
	
	pthread_mutex_lock(&lock);

	map<uint32_t, struct sockaddr_in>::iterator i;
	i = cache.find(server_id);
	if(i != cache.end()) {
		result = &i->second;
	} else {
		string ip;
		string port;
		DbCon *db = DbCon::getInstance();
		if(db) {
			ip = db->getServerAttribute(server_id, "ip");
			port = db->getServerAttribute(server_id, "udppeerport");
			
			db->release();
		}

		if(ip == "") {
			log(LOG_ERR, "Unable to obtain attribute 'ip' for server %u",
					server_id);
		} else if(port == "" || !atol(port.c_str())) {
			log(LOG_ERR, "Unable to obtain attribute 'udppeerport' for "
					"server %u", server_id);
		}else {
			struct hostent * host = gethostbyname(ip.c_str());
			if(!host) {
				log(LOG_ERR, "Error resolving '%s'", ip.c_str());
			} else if(host->h_addrtype != AF_INET) {
				log(LOG_ERR, "Invalid network type for '%s'", ip.c_str());
			} else {
				cache[server_id].sin_family = AF_INET;
				cache[server_id].sin_addr.s_addr = *(u_int32_t*)host->h_addr_list[0];
				cache[server_id].sin_port = htons(atol(port.c_str()));
				result = &cache[server_id];
				log(LOG_INFO, "Resolved %s to %s", ip.c_str(),
						inet_ntoa(result->sin_addr));
			}
		}
	}
	pthread_mutex_unlock(&lock);

	return result;
}

bool UDPPeerMessenger::sendMessage(uint32_t server_id, const Message * message) {
	const uint8_t *message_data;
	uint32_t message_len;

	if(!message->getBlob(&message_data, &message_len))
		return false;
	
	if(!message_len) {
		log(LOG_CRIT, "Message length cannot be 0 in %s", __PRETTY_FUNCTION__);
		return false;
	}

	const struct sockaddr_in *dest = getSockAddr(server_id);
	if(!dest)
		return false;
	
	// determine the number of fragments, always rounding up.
	uint16_t numfrags = (message_len + _max_fragment_size - 1) / _max_fragment_size;
	uint16_t base_frag_size = message_len / numfrags;
	uint16_t num_large_frags = message_len % numfrags;

	// See ENV_EncryptUpdate(3) for an explanation of why this needs
	// to be larger.
	uint8_t sendbuffer[BUFFER_SIZE + MAX_BLOCKSIZE];
	union {
		uint8_t buffer[BUFFER_SIZE];
		struct st_frame frame;
	};

	frame.server_id = message->server_id();
	frame.message_id = message->message_id();
	
	for(uint16_t i = 1; i <= numfrags; i++) {
		frame.fragment_num = i;
		frame.fragment_len = base_frag_size;
		if(i <= num_large_frags)
			frame.fragment_len++;

		frame.data_checksum = checksum(message_data, frame.fragment_len);
		memcpy(frame.data, message_data, frame.fragment_len);
		message_data += frame.fragment_len;

		int packetsize = frame.fragment_len + ST_FRAME_SIZE - 1;
		int sendlen;
		int tlen;
	
		pthread_mutex_lock(&_enc_lock);
		if(EVP_EncryptUpdate(&_enc_ctx, sendbuffer, &sendlen, buffer,
					packetsize) != 1) {
			pthread_mutex_unlock(&_enc_lock);
			log_ssl_errors("EVP_EncryptUpdate");
			return false;
		}
		if(EVP_EncryptFinal(&_enc_ctx, sendbuffer + sendlen, &tlen) != 1) {
			pthread_mutex_unlock(&_enc_lock);
			log_ssl_errors("EVP_EncryptFinal");
			return false;
		}
		pthread_mutex_unlock(&_enc_lock);
		sendlen += tlen;
		
		int result = sendto(_sock, sendbuffer, sendlen, 0,
				(const struct sockaddr*)dest, sizeof(*dest));
		if(result < 0) {
			lerror("sendto");
			return false;
		} else if(result < packetsize) {
			log(LOG_ERR, "Only %d bytes of %d got transmitted!  This is a serious problem!",
					result, packetsize);
		}
	}
		
	return true;
}

Message* UDPPeerMessenger::getMessage() {
	ssize_t bytes_received;
	struct sockaddr_in from;
	socklen_t fromlen = sizeof(from);
	uint8_t inbuffer[BUFFER_SIZE + MAX_BLOCKSIZE];
	union {
		uint8_t buffer[BUFFER_SIZE];
		struct st_frame frame;
	};

	Message *message = NULL;

	while(!message) {
		bytes_received = recvfrom(_sock, inbuffer, BUFFER_SIZE + MAX_BLOCKSIZE,
				MSG_TRUNC, (struct sockaddr*)&from, &fromlen);
		int packet_size;
		int tlen;
		if(bytes_received == -1) {
			if(errno != EINTR)
				lerror("recvfrom");
			return NULL;
		} else if(bytes_received > BUFFER_SIZE + MAX_BLOCKSIZE) {
			log(LOG_WARNING, "Discarding frame of size %u since it is "
					"bigger than the buffer (%d bytes)",
					(unsigned)bytes_received, BUFFER_SIZE + MAX_BLOCKSIZE);
		} else if(EVP_DecryptUpdate(&_dec_ctx, buffer, &packet_size,
					inbuffer, bytes_received) != 1) {
			log_ssl_errors("EVP_DecryptUpdate");
		} else if(EVP_DecryptFinal(&_dec_ctx, buffer + packet_size,
					&tlen) != 1) {
			log_ssl_errors("EVP_DecryptFinal");
		} else if((size_t)(packet_size + tlen) < sizeof(st_frame)) {
			log(LOG_WARNING, "Discarding frame due to short packet (%d bytes)",
					packet_size + tlen);
		} else if((size_t)(packet_size + tlen) !=
				frame.fragment_len + sizeof(st_frame) - 1) {
			log(LOG_WARNING, "Discarding frame due to invalid fragment_length field");
		} else if(frame.data_checksum != checksum(frame.data, frame.fragment_len)) {
			log(LOG_WARNING, "Discarding frame with invalid checksum");
		} else {
			NOT_IMPLEMENTED();	
		}
	}
	return message;
}

//////////////////////////////////////////////////////////////
static void log_ciphers(const OBJ_NAME *name, void*) {
	if(islower(*name->name))
		log(LOG_INFO, "%s", name->name);
}

//////////////////////////////////////////////////////////////
static UDPPeerMessenger _udpPeerMessenger;

static void udp_peer_messenger_init() __attribute__ ((constructor));
static void udp_peer_messenger_init()
{
	// double check that gcc actually did the right thing with the __attribute__ ((packed)).
	// If we don't register the messenger abacusd will detect it and abort.
	if(sizeof(st_frame) != ST_FRAME_SIZE)
		log(LOG_ERR, "Compilation error detected, sizeof(struct st_frame) should be %d but is in fact %d", ST_FRAME_SIZE, (int)sizeof(st_frame));
	else
		PeerMessenger::setMessenger(&_udpPeerMessenger);
}
