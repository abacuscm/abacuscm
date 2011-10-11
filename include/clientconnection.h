/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __CLIENTCONNECTION_H__
#define __CLIENTCONNECTION_H__

#include <stdint.h>
#include <openssl/ssl.h>
#include <string>
#include <map>
#include <pthread.h>

#include "socket.h"
#include "threadssl.h"

#define CLIENT_BFR_SIZE			512

class MessageBlock;

class ClientConnection : public Socket {
private:
	typedef std::map<std::string, uint32_t> ClientProps;

	/* The type returned by TLSv1_method and accepted by SSL_CTX_NEW changed
	 * somewhere between 0.9.8 and 1.0.0c. For now assume it was with the major
	 * revision.
	 */
#if OPENSSL_VERSION_NUMBER < 0x10000000
	static SSL_METHOD *_method;
#else
	static const SSL_METHOD *_method;
#endif
	static SSL_CTX *_context;

	/* The connection is a state machine:
	 * 1. Uninitialised: _tssl = NULL, _ssl = NULL
	 * 2. Connecting: _tssl = NULL, _ssl != NULL
	 * 3. Connected: _tssl != NULL, _ssl = NULL
	 */
	ThreadSSL *_tssl;
	SSL *_ssl;

	MessageBlock *_message;
	ClientProps _props;

	pthread_mutex_t _write_lock;

	bool initiate_ssl();
	bool process_data();
public:
	ClientConnection(int sockfd);
	virtual ~ClientConnection();

	virtual bool process();

	bool sendError(const std::string& message);
	bool reportSuccess();
	bool sendMessageBlock(const MessageBlock *mb);

	uint32_t setProperty(const std::string& prop, uint32_t value);
	uint32_t getProperty(const std::string& prop); // const ... (fails)
	uint32_t delProperty(const std::string& prop);

	static bool init();
};

#endif
