/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
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

#include "peermessenger.h"
#include "logger.h"
#include "acmconfig.h"
#include "message.h"
#include "dbcon.h"
#include "server.h"
#include "scoped_lock.h"
#include "queue.h"
#include "socket.h"
#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "threadssl.h"

#include <map>
#include <algorithm>
#include <sstream>

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

	map<uint32_t, struct addrinfo*> _addrmap;
	pthread_mutex_t _lock_addrmap;

	uint32_t checksum(const void *data, uint16_t len);
	bool startup();
	void makeACK(st_frame* frame, uint32_t server_id, uint32_t message_id);
	void sendAck(uint32_t server_id, uint32_t message_id, uint32_t to_server);
	bool sendFrame(st_frame *frame, int packetsize, const struct addrinfo* dest);

	Queue<Message*> _received_messages;

	class UDPReceiver : public Socket {
	private:
		UDTCPPeerMessenger* _messenger;
	public:
		UDPReceiver(int sock, UDTCPPeerMessenger* messenger);
		bool process();
	};

	class TCPRetriever : public Socket {
	private:
		UDTCPPeerMessenger* _messenger;
		uint32_t _server_id;
		uint32_t _message_id;
		uint32_t _size;
		Queue<uint32_t> _sq;
		set<uint32_t> _servers;
	public:
		TCPRetriever(UDTCPPeerMessenger* messenger, uint32_t server_id, uint32_t message_id, uint32_t size, uint32_t init_server);
		~TCPRetriever();

		bool process();
		void addSourceServer(uint32_t server);
	};

	// must be locked before retrieving the tcp pointer,
	// and unlocked when the pointer will no longer be used.
	// ie: overlocking, but it's the simplest solution that'll work,
	// and the ops that requires this lock is pretty fast.
	pthread_mutex_t _lock_tcp_retrievers;
	map<uint32_t, map<uint32_t, TCPRetriever*> > _tcp_retrievers;
public:
	UDTCPPeerMessenger();
	virtual ~UDTCPPeerMessenger();

	const struct addrinfo* getSockAddr(uint32_t server_id);
	void removeTCPReceiver(uint32_t server_id, uint32_t message_id);
	void notifyTCPMessage(uint32_t server_id, uint32_t message_id, uint32_t size, uint32_t sending_server);

	const unsigned char* cipher_key() const { return _cipher_key; }
	const unsigned char* cipher_iv() const { return _cipher_iv; }
	const EVP_CIPHER* cipher() const { return _cipher; }
	void deliver_message(Message* m) { _received_messages.enqueue(m); }

	virtual bool initialise();
	virtual void deinitialise();
	virtual void shutdown();
	virtual bool sendMessage(uint32_t server_id, const Message * message);
	void sendAck(uint32_t server_id, uint32_t message_id);

	virtual Message* getMessage();
};

static void log_ciphers(const OBJ_NAME *name, void*);

UDTCPPeerMessenger::UDPReceiver::UDPReceiver(int sock, UDTCPPeerMessenger *messenger)
{
	sockfd() = sock;
	_messenger = messenger;
}

bool UDTCPPeerMessenger::UDPReceiver::process()
{
	ssize_t bytes_received;
	int packet_size, tlen;
	struct sockaddr from;
	socklen_t fromlen = sizeof(from);
	uint8_t inbuffer[BUFFER_SIZE + MAX_BLOCKSIZE];
	union {
		uint8_t buffer[BUFFER_SIZE];
		struct st_frame frame;
	};

	bytes_received = recvfrom(sockfd(), inbuffer, BUFFER_SIZE + MAX_BLOCKSIZE,
			MSG_TRUNC, (struct sockaddr*)&from, &fromlen);
	if(bytes_received == -1) {
		if(errno != EINTR)
			lerror("recvfrom");
		return errno != EBADF;
	}

	if(bytes_received > BUFFER_SIZE + MAX_BLOCKSIZE) {
		log(LOG_WARNING, "Discarding frame of size %u since it is "
				"bigger than the buffer (%d bytes)",
				(unsigned)bytes_received, BUFFER_SIZE + MAX_BLOCKSIZE);
		return true;
	}

	EVP_CIPHER_CTX dec_ctx;
	EVP_CIPHER_CTX_init(&dec_ctx);
	EVP_DecryptInit(&dec_ctx, _messenger->cipher(), _messenger->cipher_key(), _messenger->cipher_iv());

	if (EVP_DecryptUpdate(&dec_ctx, buffer, &packet_size,
				inbuffer, bytes_received) != 1) {
		log_ssl_errors("EVP_DecryptUpdate");
	} else if (EVP_DecryptFinal(&dec_ctx, buffer + packet_size, &tlen) != 1) {
		log_ssl_errors("EVP_DecryptFinal");
	} else if ((size_t)(packet_size += tlen) < sizeof(st_frame)) { /* HEADS UP: packet_size __+=__ tlen */
		char host[47];
		char port[7];
		if (getnameinfo(&from, fromlen, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST) == 0)
			log (LOG_WARNING, "Discarding frame due to short packet (%d bytes), received from: %s",
					packet_size + tlen, host);
		else
			log (LOG_WARNING, "Discarding frame due to short packet (%d bytes), received from unknown host", packet_size + tlen);

		log_buffer (LOG_DEBUG, "short packet", (const unsigned char*)&frame, packet_size);
	} else if (frame.type == TYPE_INLINE ? (unsigned)packet_size != sizeof(st_frame) + frame.message_size : packet_size != sizeof(st_frame)) {
		log (LOG_WARNING, "Invalid sized packet => %d != sizeof(st_frame) + frame.message_size == %d + %d == %d (type=%d)",
				packet_size, (uint32_t)sizeof(st_frame), frame.message_size, (uint32_t)sizeof(st_frame) + frame.message_size,
				frame.type);
		log_buffer (LOG_DEBUG, "short packet", (const unsigned char*)&frame, sizeof(st_frame));
	} else {
		switch (frame.type) {
		case TYPE_ACK:
			{
				Server::putAck(frame.server_id, frame.message_id, frame.sender_id);
				// TODO: Remove from local listing.
			}; break;
		case TYPE_INLINE:
			{
				if (Server::hasMessage(frame.server_id, frame.message_id)) {
					_messenger->sendAck(frame.server_id, frame.message_id, frame.sender_id);
					break;
				}
				void *blob = malloc(frame.message_size);
				if (!blob) {
					log (LOG_CRIT, "Memory allocation error.");
					break;
				}
				memcpy(blob, frame.data, frame.message_size);
				Message *message = Message::buildMessage((uint8_t*)blob, frame.message_size);
				if (message)
					_messenger->deliver_message(message);
				else
					free(blob);
			}; break;
		case TYPE_NOTIFY:
			{
				if (Server::hasMessage(frame.server_id, frame.message_id))
					_messenger->sendAck(frame.server_id, frame.message_id, frame.sender_id);
				else
					_messenger->notifyTCPMessage(frame.server_id, frame.message_id, frame.message_size, frame.sender_id);
			}; break;
		default:
			log(LOG_WARNING, "Unknown UDP Message type for message (%d,%d) from %u.", frame.server_id, frame.message_id, frame.sender_id);
		}
	}

	EVP_CIPHER_CTX_cleanup(&dec_ctx);

	return true;
}

UDTCPPeerMessenger::TCPRetriever::TCPRetriever(UDTCPPeerMessenger* messenger, uint32_t server_id, uint32_t message_id, uint32_t size, uint32_t init_server)
{
	_messenger = messenger;
	_server_id = server_id;
	_message_id = message_id;
	_size = size;
	addSourceServer(init_server);
}

UDTCPPeerMessenger::TCPRetriever::~TCPRetriever()
{
}

bool UDTCPPeerMessenger::TCPRetriever::process()
{
	uint8_t *blob = (uint8_t*)malloc(_size);
	uint32_t pos = 0;
	ostringstream str;
	MessageBlock mb("udtcppeermessageget");
	int sock;
	SSL *ssl = NULL;
	ThreadSSL *tssl = NULL;

	if (!blob) {
		log (LOG_CRIT, "Error allocating %d bytes of memory for receiving blob.", _size);
		_messenger->removeTCPReceiver(_server_id, _message_id);
		return false;
	}

	str << _server_id;
	mb["server_id"] = str.str();
	str.str("");
	str << _message_id;
	mb["message_id"] = str.str();

	SSL_CTX *ctx = SSL_CTX_new(TLSv1_client_method());
	if(!ctx) {
		log_ssl_errors("SSL_CTX_new");
		goto quit;
	}

	if(!SSL_CTX_load_verify_locations(ctx, Config::getConfig()["udtcpmessenger"]["cacert"].c_str(), NULL)) {
		log_ssl_errors("SSL_CTX_load_verify_locations");
		goto quit;
	}

	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
	SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);

	while (pos < _size) {
		str.str(""); str << pos;
		mb["skip"] = str.str();

		int opt = 30;
		uint32_t try_server_id = _sq.dequeue();
		_sq.enqueue(try_server_id); // requeue for retry.

		// THIS IS A BUG: tcppeerport == udppeerport ... shouldn't be required.
		const struct addrinfo* serv_addr = _messenger->getSockAddr(try_server_id);
		if (!serv_addr)
			goto err;
		sock = socket(serv_addr->ai_family, SOCK_STREAM, 0);

		if (sock < 0) {
			lerror("socket");
			goto err;
		}

		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &opt, sizeof(int));
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &opt, sizeof(int));

		if (connect(sock, serv_addr->ai_addr, serv_addr->ai_addrlen) < 0) {
			lerror("connect");
			goto err;
		}

		ssl = SSL_new(ctx);
		if (!ssl) {
			log_ssl_errors("SSL_new");
			goto err;
		}

		if (!SSL_set_fd(ssl, sock)) {
			log_ssl_errors("SSL_set_fd");
			goto err;
		}

		tssl = new(nothrow) ThreadSSL(ssl);
		if (tssl == NULL) {
			log(LOG_ERR, "OOM allocating ThreadSSL");
			goto err;
		}
		ssl = NULL;    // tssl now owns it

		if (tssl->connect(ThreadSSL::BLOCK_FULL).processed < 1) {
			log_ssl_errors("SSL_connect");
			goto err;
		}

		if (!mb.writeToSSL(tssl))
			goto err;

		int rd;
		char buf[16384];

		rd = tssl->read(buf, sizeof(buf), ThreadSSL::BLOCK_PARTIAL).processed;
		if (rd > 0) {
			string headers(buf, rd);
			if (headers.substr(headers.length() - 2) != "\n\n") {
				log (LOG_CRIT, "crazy heuristic broke in %s (%s:%d)", __PRETTY_FUNCTION__, __FILE__, __LINE__);
				goto quit;
			}
		}

		pos = tssl->read(blob, _size, ThreadSSL::BLOCK_FULL).processed;
err:
		if (tssl)
			delete tssl;

		if (ssl)
			SSL_free(ssl);

		if (sock > 0)
			close(sock);
	}

	if (pos == _size) {
		Message*m = Message::buildMessage(blob, _size);
		if (m)
			_messenger->deliver_message(m);
		blob = NULL;
	}
quit:
	_messenger->removeTCPReceiver(_server_id, _message_id);
	if (blob)
		free(blob);
	if (ctx)
		SSL_CTX_free(ctx);
	return false;
}

void UDTCPPeerMessenger::TCPRetriever::addSourceServer(uint32_t server)
{
	if (_servers.find(server) == _servers.end()) {
		_servers.insert(server);
		_sq.enqueue(server);
	}
}

void UDTCPPeerMessenger::removeTCPReceiver(uint32_t server_id, uint32_t message_id)
{
	ScopedLock _(&_lock_tcp_retrievers);
	map<uint32_t, TCPRetriever*>::iterator tr = _tcp_retrievers[server_id].find(message_id);
	if (tr != _tcp_retrievers[server_id].end())
		_tcp_retrievers[server_id].erase(tr);
	else
		log(LOG_WARNING, "Attempt to remove non-existant TCPReceiver from TCPReceiver map.");
}

void UDTCPPeerMessenger::notifyTCPMessage(uint32_t server_id, uint32_t message_id, uint32_t size, uint32_t sending_server)
{
	ScopedLock _(&_lock_tcp_retrievers);
	map<uint32_t, TCPRetriever*>::iterator tr = _tcp_retrievers[server_id].find(message_id);
	if (tr != _tcp_retrievers[server_id].end())
		tr->second->addSourceServer(sending_server);
	else
		Server::putSocket(_tcp_retrievers[server_id][message_id] = new TCPRetriever(this, server_id, message_id, size, sending_server), true);
}

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
	pthread_mutex_init(&_lock_tcp_retrievers, NULL);
}

UDTCPPeerMessenger::~UDTCPPeerMessenger() {
	if(_sock > 0)
		deinitialise();
	pthread_mutex_destroy(&_lock_addrmap);
	pthread_mutex_destroy(&_lock_tcp_retrievers);

	map<uint32_t, struct addrinfo*>::iterator i;
	for (i = _addrmap.begin(); i != _addrmap.end(); ++i)
		freeaddrinfo(i->second);
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
	struct addrinfo hints;
	struct addrinfo *adinf = NULL;
	struct addrinfo *i;

	Config &config = Config::getConfig();
	string cipher_string = config["udtcpmessenger"]["cipher"];
	string cipher_key = config["udtcpmessenger"]["keyfile"];
	string cipher_iv = config["udtcpmessenger"]["ivfile"];
	string bind = config["udtcpmessenger"]["bind"];
	string port = config["udtcpmessenger"]["port"];

	int fd;

	if (bind == "")
		bind = "any";

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM;

	if (getaddrinfo(bind.c_str(), port.c_str(), &hints, &adinf)) {
		lerror("getaddrinfo");
		return false;
	}

	for (i = adinf; i && _sock <= 0; i = i->ai_next) {
		_sock = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
		if (_sock < 0) {
			lerror("socket");
			continue;
		}

		if (::bind(_sock, i->ai_addr, i->ai_addrlen) < 0) {
			lerror("bind");
			close(_sock);
			_sock = -1;
			continue;
		}
	}

	freeaddrinfo(adinf);

	if (_sock < 0) {
		log(LOG_ERR, "Unable to bind udp port for udtcpmessenger, bombing out.");
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

	Server::putSocket(new UDPReceiver(_sock, this));

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

const struct addrinfo* UDTCPPeerMessenger::getSockAddr(uint32_t server_id) {
	ScopedLock _(&_lock_addrmap);

	map<uint32_t, struct addrinfo*>::iterator i;
	i = _addrmap.find(server_id);
	if(i != _addrmap.end()) {
		return i->second;
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
			return NULL;
		} else if(port == "") {
			log(LOG_ERR, "Unable to obtain attribute 'udppeerport' for "
					"server %u", server_id);
			return NULL;
		}else {
			struct addrinfo hints;
			struct addrinfo *adinf = NULL;
			int e;

			memset(&hints, 0, sizeof(hints));

			if ((e = getaddrinfo(ip.c_str(), port.c_str(), &hints, &adinf)) != 0) {
				log(LOG_ERR, "Error obtaining sockaddr data for %s:%s: %s.", ip.c_str(), port.c_str(), gai_strerror(e));
				return NULL;
			}

			return _addrmap[server_id] = adinf;
		}
	}
}

bool UDTCPPeerMessenger::sendFrame(st_frame *buffer, int packetsize, const struct addrinfo* dest) {
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
			dest->ai_addr, dest->ai_addrlen);
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
	frame->message_size = 0;
	frame->type = TYPE_ACK;
	frame->checksum = 0;
	frame->checksum = checksum(&frame, sizeof(st_frame));
}

void UDTCPPeerMessenger::sendAck(uint32_t server_id, uint32_t message_id) {
	if (!Server::getId())
		return;

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
		const addrinfo * dest = getSockAddr(*i);
		if (!dest)
			return;
		if (!sendFrame(&frame, sizeof(st_frame), dest))
			log(LOG_WARNING, "Error sending ACK for (%u,%u) to %u.",
					server_id, message_id, *i);
	}
}

void UDTCPPeerMessenger::sendAck(uint32_t server_id, uint32_t message_id, uint32_t to_server) {
	if (!Server::getId())
		return;

	st_frame frame;
	const addrinfo * dest = getSockAddr(to_server);
	if (!dest)
		return;

	makeACK(&frame, server_id, message_id);

	if(!sendFrame(&frame, sizeof(st_frame), dest))
		log(LOG_WARNING, "Error sending ACK for (%u,%u) to %u.",
				server_id, message_id, to_server);
}

bool UDTCPPeerMessenger::sendMessage(uint32_t server_id, const Message * message)
{
	uint32_t transmit_size;
	const uint8_t *message_data;
	const struct addrinfo *dest = getSockAddr(server_id);
	union {
		uint8_t buffer[BUFFER_SIZE];
		struct st_frame frame;
	};

	// fudge that buffer is used.
	if (0) buffer[0] = 0;

	if(!dest)
		return false;

	if(!message->getBlob(&message_data, &frame.message_size))
		return false;

	if(!frame.message_size) {
		log(LOG_CRIT, "Message length cannot be 0 in %s", __PRETTY_FUNCTION__);
		return false;
	}

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

class TCPTransmitAction : public ClientAction {
private:
	struct MessageContainer {
		uint8_t *message_blob;
		uint32_t blob_size;
		int rfcnt;

		MessageContainer(const uint8_t *blob, uint32_t size) {
			rfcnt = 1;
			message_blob = new uint8_t[size];
			blob_size = size;
			memcpy(message_blob, blob, size);
		}

		~MessageContainer() {
			delete []message_blob;
		}
	};
	typedef map<uint32_t, MessageContainer*> MessageMap;
	typedef map<uint32_t, MessageMap> ServerMessageMap;

	ServerMessageMap _message_map;
	pthread_mutex_t _lock_map;

	uint32_t auth_cred;
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
public:
	TCPTransmitAction();
	~TCPTransmitAction();
};

TCPTransmitAction::TCPTransmitAction()
{
	pthread_mutex_init(&_lock_map, NULL);
}

TCPTransmitAction::~TCPTransmitAction()
{
#if 1
	ServerMessageMap::iterator i;
	MessageMap::iterator j;
	for (i = _message_map.begin(); i != _message_map.end(); ++i)
		for (j = i->second.begin(); j != i->second.end(); ++j)
			log (LOG_WARNING, "Non-destructed Message object in TCPTransmitAction ... this indicates a bug!!!!!!");
#endif
	pthread_mutex_destroy(&_lock_map);
}

bool TCPTransmitAction::int_process(ClientConnection *cc, MessageBlock *mb)
{
#if 0
	// NOTE: This authentication is WEAK!! TODO
	uint32_t remote_id = strtoull((*mb)["requester_id"].c_str(), NULL, 10);
	uint32_t creds = strtoull((*mb)["cred"].c_str(), NULL, 10) ^ remote_id ^ auth_cred ^ Server::getId();
	if (!remote_id || creds)
		return false;
#endif

	uint32_t server_id = strtoull((*mb)["server_id"].c_str(), NULL, 10);
	uint32_t message_id = strtoull((*mb)["message_id"].c_str(), NULL, 10);
	uint32_t skip = strtoull((*mb)["skip"].c_str(), NULL, 10);

	if (!server_id || !message_id)
		return false;

	MessageContainer *mc = NULL;
	{
		ScopedLock _(&_lock_map);

		ServerMessageMap::iterator s = _message_map.find(server_id);
		if (s != _message_map.end()) {
			MessageMap::iterator m = s->second.find(message_id);
			if (m != s->second.end()) {
				mc = m->second;
				++mc->rfcnt;
			}
		}

		if (!mc) {
			DbCon *db = DbCon::getInstance();
			if (!db)
				return false;

			ostringstream query;
			query << "SELECT message_type_id, time, signature, data FROM PeerMessage WHERE server_id=" << server_id << " AND message_id=" << message_id;

			QueryResultRow row = db->singleRowQuery(query.str());
			db->release(); db = NULL;

			if (row.empty())
				return false;

			uint8_t *signature = new uint8_t[row[2].length()];
			uint8_t *data = new uint8_t[row[3].length()];

			memcpy(signature, row[2].data(), row[2].length());
			memcpy(data, row[3].data(), row[3].length());

			Message* m = Message::buildMessage(server_id, message_id, atol(row[0].c_str()), atol(row[1].c_str()), signature, data, row[3].length());

			if (m) {
				const uint8_t *blob;
				uint32_t size;
				if (m->getBlob(&blob, &size))
					_message_map[server_id][message_id] = mc = new MessageContainer(blob, size);
				delete m;
			} else {
				delete []signature;
				delete []data;
			}
		}
	}

	ostringstream str;

	bool ret;
	if (skip < mc->blob_size) {
		MessageBlock rt("udtcppeermessageput");
		rt.setContent((char*)mc->message_blob + skip, mc->blob_size - skip, false); /* SHARED DATA COPY */
		ret = cc->sendMessageBlock(&rt);
	} else
		ret = false;

	{
		ScopedLock _(&_lock_map);
		if (!--mc->rfcnt) {
			delete mc;
			ServerMessageMap::iterator s = _message_map.find(server_id);
			MessageMap::iterator m = s->second.find(message_id);
			s->second.erase(m);
			if (s->second.empty())
				_message_map.erase(s);
		}
	}

	return ret;
}

static TCPTransmitAction _act_getmsg;

//////////////////////////////////////////////////////////////
static void log_ciphers(const OBJ_NAME *name, void*) {
	if(islower(*name->name))
		log(LOG_INFO, "%s", name->name);
}

//////////////////////////////////////////////////////////////
static UDTCPPeerMessenger _udtcpPeerMessenger;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("udtcppeermessageget", !PERMISSION_AUTH, &_act_getmsg);
	// double check that gcc actually did the right thing with the __attribute__ ((packed)).
	// If we don't register the messenger abacusd will detect it and abort.
	if(sizeof(st_frame) != ST_FRAME_SIZE)
		log(LOG_ERR, "Compilation error detected, sizeof(struct st_frame) should be %d but is in fact %d", ST_FRAME_SIZE, (int)sizeof(st_frame));
	else
		PeerMessenger::setMessenger(&_udtcpPeerMessenger);
}
