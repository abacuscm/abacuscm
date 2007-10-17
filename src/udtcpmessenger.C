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

#include "peermessenger.h"
#include "logger.h"
#include "acmconfig.h"
#include "message.h"
#include "dbcon.h"
#include "server.h"
#include "scoped_lock.h"
#include "queue.h"

#include <map>
#include <algorithm>

using namespace std;

#define ST_FRAME_SIZE		21

#define BUFFER_SIZE					5120
#define MAX_UDP_MESSAGE_SIZE		((unsigned)(BUFFER_SIZE - sizeof(st_frame)))
#define MAX_BLOCKSIZE				32

#define TYPE_ACK					1
#define TYPE_INLINE					2
#define TYPE_NOTIFY					3

struct st_frame {
	uint32_t checksum;
	uint32_t sender_id; // sending server_id
	uint32_t server_id;
	uint32_t message_id;
	uint32_t message_size;
	uint8_t type;
	uint8_t data[0];
} __attribute__ ((packed));

class UDTCPPeerMessenger : public PeerMessenger {
private:
	int _sock;
	uint32_t _checksum_seed;
	const EVP_CIPHER *_cipher;
	unsigned char* _cipher_key;
	unsigned char* _cipher_iv;
	int _cipher_blocksize;
	int _cipher_ivsize;
	int _cipher_keysize;

	map<uint32_t, struct sockaddr_in> _addrmap;
	pthread_mutex_t _lock_addrmap;

	const sockaddr_in* getSockAddr(uint32_t server_id);
	uint32_t checksum(const void *data, uint16_t len);
	bool startup();
	void makeACK(st_frame* frame, uint32_t server_id, uint32_t message_id);
	void sendAck(uint32_t server_id, uint32_t message_id, uint32_t to_server);
	bool sendFrame(st_frame *frame, int packetsize, const struct sockaddr_in* dest);

	Queue<Message*> _received_messages;
public:
	UDTCPPeerMessenger();
	virtual ~UDTCPPeerMessenger();

	virtual bool initialise();
	virtual void deinitialise();
	virtual void shutdown();
	virtual bool sendMessage(uint32_t server_id, const Message * message);
	void sendAck(uint32_t server_id, uint32_t message_id);

	virtual Message* getMessage();
};

static void log_ciphers(const OBJ_NAME *name, void*);

UDTCPPeerMessenger::UDTCPPeerMessenger() {
	_sock = -1;
	_checksum_seed = 0;
	_cipher = NULL;
	_cipher_key = NULL;
	_cipher_iv = NULL;
	_cipher_blocksize = -1;
	_cipher_ivsize = -1;
	_cipher_keysize = -1;
	pthread_mutex_init(&_lock_addrmap, NULL);
}

UDTCPPeerMessenger::~UDTCPPeerMessenger() {
	if(_sock > 0)
		deinitialise();
	pthread_mutex_destroy(&_lock_addrmap);
}

bool UDTCPPeerMessenger::initialise() {
	_checksum_seed = atoll(Config::getConfig()["udtcpmessenger"]["checksumseed"].c_str());
	log(LOG_DEBUG, "Using %u as checksum seed", _checksum_seed);

	if(_sock < 0)
		return startup();

	return true;
}

void UDTCPPeerMessenger::deinitialise() {
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

void UDTCPPeerMessenger::shutdown()
{
	_received_messages.enqueue(NULL);
}

bool UDTCPPeerMessenger::startup() {
	struct sockaddr_in sock_addr;
	uint16_t portnum;
	Config &config = Config::getConfig();
	string cipher_string = config["udtcpmessenger"]["cipher"];
	string cipher_key = config["udtcpmessenger"]["keyfile"];
	string cipher_iv = config["udtcpmessenger"]["ivfile"];
	int fd;

	_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if(_sock < 0) {
		lerror("socket");
		goto err;
	}

	portnum = atol(config["udtcpmessenger"]["port"].c_str());
	if(!portnum)
		portnum = DEFAULT_UDPRECEIVER_PORT;

	log(LOG_INFO, "Binding UDTCP Messenger to UDP port %d.", portnum);
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(portnum);
	sock_addr.sin_addr.s_addr = INADDR_ANY;

	if(bind(_sock, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) < 0) {
		lerror("bind");
		goto err;
	}

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

	log(LOG_INFO, "UDTCPPeerMessenger started up");
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

uint32_t UDTCPPeerMessenger::checksum(const void *_data, uint16_t len) {
	const uint8_t *data = (const uint8_t*)_data;
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

const sockaddr_in* UDTCPPeerMessenger::getSockAddr(uint32_t server_id) {
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

bool UDTCPPeerMessenger::sendFrame(st_frame *buffer, int packetsize, const struct sockaddr_in* dest) {
	int sendlen;
	int tlen;

	// See ENV_EncryptUpdate(3) for an explanation of why this needs
	// to be larger.
	uint8_t sendbuffer[BUFFER_SIZE + MAX_BLOCKSIZE];

	EVP_CIPHER_CTX enc_ctx;

	EVP_CIPHER_CTX_init(&enc_ctx);
	EVP_EncryptInit(&enc_ctx, _cipher, _cipher_key, _cipher_iv);

	if(EVP_EncryptUpdate(&enc_ctx, sendbuffer, &sendlen, (unsigned char*)buffer, packetsize) != 1) {
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

void UDTCPPeerMessenger::makeACK(st_frame* frame, uint32_t server_id, uint32_t message_id)
{
	frame->sender_id = Server::getId();
	frame->server_id = server_id;
	frame->message_id = message_id;
	frame->type = TYPE_ACK;
	frame->checksum = 0;
	frame->checksum = checksum(&frame, sizeof(st_frame));
}

void UDTCPPeerMessenger::sendAck(uint32_t server_id, uint32_t message_id) {
	st_frame frame;
	DbCon *db = DbCon::getInstance();
	if(!db) {
		log(LOG_ERR, "Not global acking (%u,%u) - unable to obtain DbCon",
				server_id, message_id);
		return;
	}
	vector<uint32_t> servers = db->getRemoteServers();
	db->release();db=NULL;

	makeACK(&frame, server_id, message_id);

	vector<uint32_t>::iterator i;
	for(i = servers.begin(); i != servers.end(); ++i) {
		const sockaddr_in * dest = getSockAddr(*i);
		if (!sendFrame(&frame, message_id, dest))
			log(LOG_WARNING, "Error sending ACK for (%u,%u) to %u.",
					server_id, message_id, *i);
	}
}

void UDTCPPeerMessenger::sendAck(uint32_t server_id, uint32_t message_id, uint32_t to_server) {
	st_frame frame;
	const sockaddr_in * dest = getSockAddr(to_server);

	makeACK(&frame, server_id, message_id);

	if(!sendFrame(&frame, sizeof(st_frame), dest))
		log(LOG_WARNING, "Error sending ACK for (%u,%u) to %u.",
				server_id, message_id, to_server);
}

bool UDTCPPeerMessenger::sendMessage(uint32_t server_id, const Message * message)
{
	uint32_t transmit_size;
	const uint8_t *message_data;
	const struct sockaddr_in *dest = getSockAddr(server_id);
	union {
		uint8_t buffer[BUFFER_SIZE];
		struct st_frame frame;
	};

	// fudge that buffer is used.
	if (0) buffer[0] = 0;

	if(!dest)
		return false;

	if(!frame.message_size) {
		log(LOG_CRIT, "Message length cannot be 0 in %s", __PRETTY_FUNCTION__);
		return false;
	}

	if(!message->getBlob(&message_data, &frame.message_size))
		return false;

	frame.sender_id = Server::getId();
	frame.server_id = message->server_id();
	frame.message_id = message->message_id();
	// frame.message_size already set

	if (frame.message_size < MAX_UDP_MESSAGE_SIZE) {
		frame.type = TYPE_INLINE;
		memcpy(frame.data, message_data, frame.message_size);
		transmit_size = sizeof(st_frame) + frame.message_size;
	} else {
		frame.type = TYPE_NOTIFY;
		transmit_size = sizeof(st_frame);
	}

	frame.checksum = 0;
	frame.checksum = checksum(&frame, transmit_size);

	if(!sendFrame(&frame, transmit_size, dest))
		log(LOG_WARNING, "Failed to send %s (%u,%u) to server %u",
				frame.type == TYPE_NOTIFY ? "message notification" : "message content",
				frame.server_id, frame.message_id, server_id);

	return true;
}

Message* UDTCPPeerMessenger::getMessage() {
	return _received_messages.dequeue();
}

#if 0
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
#endif

//////////////////////////////////////////////////////////////
static void log_ciphers(const OBJ_NAME *name, void*) {
	if(islower(*name->name))
		log(LOG_INFO, "%s", name->name);
}

//////////////////////////////////////////////////////////////
static UDTCPPeerMessenger _udtcpPeerMessenger;

static void udtcp_peer_messenger_init() __attribute__ ((constructor));
static void udtcp_peer_messenger_init()
{
	// double check that gcc actually did the right thing with the __attribute__ ((packed)).
	// If we don't register the messenger abacusd will detect it and abort.
	if(sizeof(st_frame) != ST_FRAME_SIZE)
		log(LOG_ERR, "Compilation error detected, sizeof(struct st_frame) should be %d but is in fact %d", ST_FRAME_SIZE, (int)sizeof(st_frame));
	else
		PeerMessenger::setMessenger(&_udtcpPeerMessenger);
}
