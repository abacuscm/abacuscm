/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif
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
#include <pthread.h>
#include <signal.h>

#include "peermessenger.h"
#include "logger.h"
#include "acmconfig.h"
#include "message.h"
#include "dbcon.h"
#include "server.h"
#include "scoped_lock.h"

#include <map>
#include <algorithm>

using namespace std;

#define ST_FRAME_SIZE		19

#define MIN_FRAGMENT_SIZE			200
#define DEFAULT_MAX_FRAGMENT_SIZE	1000
#define BUFFER_SIZE					1408
#define MAX_FRAGMENT_SIZE			((unsigned)(BUFFER_SIZE - sizeof(st_frame) + 1))
#define MAX_BLOCKSIZE				32

struct st_frame {
	uint32_t server_id;
	uint32_t message_id;
	uint16_t fragment_num;
	union { // when fragment_num == 0 it indicates an ACK, in which case acking_server
			// should be used, when > 0 fragment_len is valid.
			// If server_id and message_id is also 0 it's considered a general poll
			// (I'm an 'alive' message).
		uint32_t fragment_len;
		uint32_t acking_server;
	};
	uint32_t data_checksum;
	uint8_t data[1];
} __attribute__ ((packed));

struct st_fragment {
	uint16_t fragment_length;
	uint8_t data[1];
} __attribute__ ((packed));

typedef map<uint16_t, st_fragment*> FragmentList;

struct st_fragment_collection {
	uint16_t num_fragments;
	uint16_t fragments_received;
	FragmentList fragments;
};

typedef map<uint32_t, st_fragment_collection> ServerFragmentMap;
typedef map<uint32_t, ServerFragmentMap> FragmentMap;

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

	uint32_t _min_delay;
	uint32_t _max_delay;
	uint32_t _init_delay;

	FragmentMap _fragments;
	pthread_t _receiver_thread;

	map<uint32_t, struct sockaddr_in> _addrmap;
	pthread_mutex_t _lock_addrmap;

	map<uint32_t, uint32_t> _backoffmap;
	pthread_mutex_t _lock_backoffmap;

	const sockaddr_in* getSockAddr(uint32_t server_id);
	uint32_t getBackoff(uint32_t server_id); // also ups the backoff.
	void downBackoff(uint32_t server_id);

	uint32_t checksum(const uint8_t *data, uint16_t len);

	bool startup();

	bool sendFrame(uint8_t *buffer, int packetsize,
			const struct sockaddr_in* dest);
	void sendAck(uint32_t server_id, uint32_t message_id, uint32_t to_server);
public:
	UDPPeerMessenger();
	virtual ~UDPPeerMessenger();

	virtual bool initialise();
	virtual void deinitialise();
	virtual void shutdown();
	virtual bool sendMessage(uint32_t server_id, const Message * message);
	void sendAck(uint32_t server_id, uint32_t message_id);
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
	pthread_mutex_init(&_lock_addrmap, NULL);
	pthread_mutex_init(&_lock_backoffmap, NULL);
}

UDPPeerMessenger::~UDPPeerMessenger() {
	deinitialise();
	pthread_mutex_destroy(&_lock_addrmap);
	pthread_mutex_destroy(&_lock_backoffmap);
}

bool UDPPeerMessenger::initialise() {
	_checksum_seed = atoll(Config::getConfig()["udpmessenger"]["checksumseed"].c_str());
	log(LOG_DEBUG, "Using %u as checksum seed", _checksum_seed);

	_init_delay = atoll(Config::getConfig()["udpmessenger"]["init_delay"].c_str());
	_min_delay = atoll(Config::getConfig()["udpmessenger"]["min_delay"].c_str());
	_max_delay = atoll(Config::getConfig()["udpmessenger"]["max_delay"].c_str());

	if (_min_delay < 100) {
		_min_delay = 100;
		log(LOG_NOTICE, "Increasing min_delay to 100us.");
	}

	if (_max_delay < 1000 * _min_delay) {
		_max_delay = 1000 * _min_delay;
		log(LOG_NOTICE, "Increasing max_delay to %uus.", _max_delay);
	}

	if (_init_delay < _min_delay || _init_delay > _max_delay) {
		_init_delay = _min_delay / 2 + _max_delay / 2;
		log(LOG_NOTICE, "init_delay out of range, using average of min_delay and max_delay (%uus).", _init_delay);
	}

	log(LOG_INFO, "Using backoff values (%u, %u, %u).", _min_delay, _init_delay, _max_delay);

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
}

void UDPPeerMessenger::shutdown() {
	pthread_kill(_receiver_thread, SIGTERM);
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
		lerror("open (cipher_key)");
		goto err;
	}

	_cipher_key = (unsigned char*)mmap(NULL, _cipher_keysize, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if(_cipher_key == MAP_FAILED) {
		lerror("mmap");
		goto err;
	}

	fd = open(cipher_iv.c_str(), O_RDONLY);
	if(fd < 0) {
		lerror("open (cipher_iv)");
		goto err;
	}

	_cipher_iv = (unsigned char*)mmap(NULL, _cipher_ivsize, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if(_cipher_iv == MAP_FAILED) {
		lerror("mmap");
		goto err;
	}

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
	sockaddr_in *result = NULL;

	pthread_mutex_lock(&_lock_addrmap);

	map<uint32_t, struct sockaddr_in>::iterator i;
	i = _addrmap.find(server_id);
	if(i != _addrmap.end()) {
		result = &i->second;
	} else {
		string ip;
		string port;
		DbCon *db = DbCon::getInstance();
		if(db) {
			ip = db->getServerAttribute(server_id, "ip");
			port = db->getServerAttribute(server_id, "udppeerport");

			db->release();db=NULL;
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
				_addrmap[server_id].sin_family = AF_INET;
				_addrmap[server_id].sin_addr.s_addr = *(u_int32_t*)host->h_addr_list[0];
				_addrmap[server_id].sin_port = htons(atol(port.c_str()));
				result = &_addrmap[server_id];
				log(LOG_INFO, "Resolved %s to %s", ip.c_str(),
						inet_ntoa(result->sin_addr));
			}
		}
	}
	pthread_mutex_unlock(&_lock_addrmap);

	return result;
}

uint32_t UDPPeerMessenger::getBackoff(uint32_t server_id)
{
	ScopedLock _(&_lock_backoffmap);

	uint32_t result;
	map<uint32_t, uint32_t>::iterator i;
	i = _backoffmap.find(server_id);
	if(i == _backoffmap.end()) {
		result = _backoffmap[server_id] = _init_delay;
		log(LOG_DEBUG, "Initialised backoff for server_id=%u to %uus.", server_id, result);
	} else {
		result = i->second;
		if ((i->second *= 2) > _max_delay)
			i->second = _max_delay;
		if (i->second != result)
			log(LOG_DEBUG, "Increased backoff for server_id=%u to %uus.", server_id, i->second);
	}

	return result;
}

void UDPPeerMessenger::downBackoff(uint32_t server_id)
{
	ScopedLock _(&_lock_backoffmap);

	map<uint32_t, uint32_t>::iterator i;
	i = _backoffmap.find(server_id);
	if(i != _backoffmap.end()) {
		uint32_t otime = i->second;
		if((i->second /= 2) < _min_delay)
			i->second = _min_delay;
		if (otime != i->second)
			log(LOG_DEBUG, "Decreased backoff for server_id=%u to %uus.", server_id, i->second);
	}
}

bool UDPPeerMessenger::sendFrame(uint8_t *buffer, int packetsize,
		const struct sockaddr_in* dest) {
	int sendlen;
	int tlen;

	// See ENV_EncryptUpdate(3) for an explanation of why this needs
	// to be larger.
	uint8_t sendbuffer[BUFFER_SIZE + MAX_BLOCKSIZE];

	EVP_CIPHER_CTX enc_ctx;

	EVP_CIPHER_CTX_init(&enc_ctx);
	EVP_EncryptInit(&enc_ctx, _cipher, _cipher_key, _cipher_iv);

	if(EVP_EncryptUpdate(&enc_ctx, sendbuffer, &sendlen, buffer,
				packetsize) != 1) {
		log_ssl_errors("EVP_EncryptUpdate");
		EVP_CIPHER_CTX_cleanup(&enc_ctx);
		return false;
	}
	if(EVP_EncryptFinal(&enc_ctx, sendbuffer + sendlen, &tlen) != 1) {
		log_ssl_errors("EVP_EncryptFinal");
		EVP_CIPHER_CTX_cleanup(&enc_ctx);
		return false;
	}
	sendlen += tlen;
	EVP_CIPHER_CTX_cleanup(&enc_ctx);

	int result = sendto(_sock, sendbuffer, sendlen, 0,
			(const struct sockaddr*)dest, sizeof(*dest));
	if(result < 0) {
		lerror("sendto");
		return false;
	} else if(result < packetsize) {
		log(LOG_ERR, "Only %d bytes of %d got transmitted!  This is a serious problem!",
				result, packetsize);
		return false;
	}
	return true;
}

void UDPPeerMessenger::sendAck(uint32_t server_id, uint32_t message_id) {
	DbCon *db = DbCon::getInstance();
	if(!db) {
		log(LOG_ERR, "Not global acking (%u,%u) - unable to obtain DbCon",
				server_id, message_id);
		return;
	}
	vector<uint32_t> servers = db->getRemoteServers();
	db->release();db=NULL;

	vector<uint32_t>::iterator i;
	for(i = servers.begin(); i != servers.end(); ++i)
		sendAck(server_id, message_id, *i);
}

void UDPPeerMessenger::sendAck(uint32_t server_id, uint32_t message_id,
		uint32_t to_server) {
	union {
		st_frame frame;
		uint8_t buffer[1];
	};
	const sockaddr_in * dest = getSockAddr(to_server);

	frame.server_id = server_id;
	frame.message_id = message_id;
	frame.fragment_num = 0;
	frame.acking_server = Server::getId();
	frame.data_checksum = checksum(NULL, 0);
	*frame.data = 0;

	if(!sendFrame(buffer, sizeof(st_frame), dest))
		log(LOG_WARNING, "Error sending ACK for (%u,%u) to %u.",
				server_id, message_id, to_server);
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
	uint32_t base_frag_size = message_len / numfrags;
	uint16_t num_large_frags = message_len % numfrags;

	union {
		uint8_t buffer[BUFFER_SIZE];
		struct st_frame frame;
	};

	frame.server_id = message->server_id();
	frame.message_id = message->message_id();

	if((numfrags & (1 << (sizeof(frame.fragment_len) * 8 - 1))) != 0) {
		log(LOG_CRIT, "Too many fragments!  Message(%u,%u)", frame.server_id, frame.message_id);
		return false;
	}

	uint32_t backoff = getBackoff(server_id);

	vector<uint16_t> fragnums;
	fragnums.reserve(numfrags);
	for(uint16_t i = 1; i <= numfrags; ++i)
		fragnums.push_back(i);
	random_shuffle(fragnums.begin(), fragnums.end());

	for(uint16_t c = 0; c < numfrags; ++c) {
		uint16_t i = fragnums[c];

		frame.fragment_num = i;
		if(i == numfrags)
			frame.fragment_num |= 1 << (sizeof(frame.fragment_num) * 8 - 1);

		const uint8_t* frag_data = message_data + base_frag_size * (i - 1U) + min((uint16_t)(i - 1U), num_large_frags);

		frame.fragment_len = base_frag_size;
		if(i <= num_large_frags)
			frame.fragment_len++;

		frame.data_checksum = checksum(frag_data, frame.fragment_len);
		memcpy(frame.data, frag_data, frame.fragment_len);

		int packetsize = frame.fragment_len + ST_FRAME_SIZE - 1;

		if (c)
			usleep(backoff);

		if(!sendFrame(buffer, packetsize, dest))
			log(LOG_WARNING, "Frame %u for (%u,%u) destined for %u failed "
					"to transmit", frame.fragment_num, frame.server_id,
					frame.message_id, server_id);
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
	_receiver_thread = pthread_self();

	while (!message) {
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
		} else {
			EVP_CIPHER_CTX dec_ctx;
			EVP_CIPHER_CTX_init(&dec_ctx);
			EVP_DecryptInit(&dec_ctx, _cipher, _cipher_key, _cipher_iv);

			if(EVP_DecryptUpdate(&dec_ctx, buffer, &packet_size,
						inbuffer, bytes_received) != 1) {
				log_ssl_errors("EVP_DecryptUpdate");
			} else if(EVP_DecryptFinal(&dec_ctx, buffer + packet_size,
						&tlen) != 1) {
				log_ssl_errors("EVP_DecryptFinal");
			} else if((size_t)(packet_size + tlen) < sizeof(st_frame)) {
				log(LOG_WARNING, "Discarding frame due to short packet (%d bytes)",
						packet_size + tlen);
			} else if(frame.fragment_num == 0) {
//				log(LOG_INFO, "Received ACK for (%u,%u) from %u",
//						frame.server_id, frame.message_id, frame.acking_server);
				Server::putAck(frame.server_id, frame.message_id,
						frame.acking_server);
				downBackoff(frame.acking_server);
			} else if((size_t)(packet_size + tlen) !=
					frame.fragment_len + sizeof(st_frame) - 1) {
				log(LOG_WARNING, "Discarding frame due to invalid fragment_length field");
			} else if(frame.data_checksum != checksum(frame.data, frame.fragment_len)) {
				log(LOG_WARNING, "Discarding frame with invalid checksum");
			} else if(Server::hasMessage(frame.server_id, frame.message_id)) {
//				log(LOG_INFO, "Received fragment for (%u,%u) which we already have.", frame.server_id, frame.message_id);
				sendAck(frame.server_id, frame.message_id);
			} else {
				bool lastfragment = (frame.fragment_num & (1 << (sizeof(frame.fragment_num) * 8 - 1))) != 0;
				frame.fragment_num &= (1 << (sizeof(frame.fragment_num) * 8 - 1)) - 1;

//				log(LOG_DEBUG, "Received fragment %u for (%u,%u)%s.", frame.fragment_num, frame.server_id, frame.message_id, lastfragment ? " - last fragment" : "");

				ServerFragmentMap::iterator i = _fragments[frame.server_id].find(frame.message_id);

				if(i == _fragments[frame.server_id].end()) {
					_fragments[frame.server_id][frame.message_id].num_fragments = 0;
					_fragments[frame.server_id][frame.message_id].fragments_received = 0;
				}

				st_fragment_collection *frags = &_fragments[frame.server_id][frame.message_id];

				if(frags->num_fragments <= frame.fragment_num)
					frags->num_fragments = frame.fragment_num + (lastfragment ? 0 : 1);

				FragmentList::iterator fragpos = frags->fragments.find(frame.fragment_num);
				if(fragpos != frags->fragments.end()) {
					log(LOG_WARNING, "Duplicate fragment (%u,%u,%u)", frame.server_id, frame.message_id, frame.fragment_num);
				} else {
					st_fragment *fragment = (st_fragment*)malloc(frame.fragment_len + sizeof(st_fragment) - 1);

					fragment->fragment_length = frame.fragment_len;
					memcpy(fragment->data, frame.data, frame.fragment_len);

					frags->fragments_received++;
					frags->fragments[frame.fragment_num] = fragment;

					if(frags->fragments_received == frags->num_fragments) {
						log(LOG_DEBUG, "Received message (%u,%u)", frame.server_id, frame.message_id);

						uint32_t total_len = 0;
						FragmentList::iterator i = frags->fragments.begin();
						while(i != frags->fragments.end()) {
							total_len += i->second->fragment_length;
							++i;
						}

						uint8_t *blob = (uint8_t*)malloc(total_len);
						uint8_t *pos = blob;
						for(i = frags->fragments.begin(); i != frags->fragments.end(); ++i) {
							memcpy(pos, i->second->data, i->second->fragment_length);
							pos += i->second->fragment_length;
						}

						message = Message::buildMessage(blob, total_len);
						if(!message) {
							log(LOG_CRIT, "Error reconstructing message (%u,%u)", frame.server_id, frame.message_id);
							free(blob);
						} else {
							for(i = frags->fragments.begin(); i != frags->fragments.end(); ++i) {
								free(i->second);
							}
							_fragments[frame.server_id].erase(frame.message_id);
						}
					}
				}
			}

			EVP_CIPHER_CTX_cleanup(&dec_ctx);
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
