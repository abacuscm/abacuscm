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
#include "permissions.h"

#define CLIENT_BFR_SIZE			512

class MessageBlock;

class ClientConnection : public Socket {
private:
	static const SSL_METHOD *_method;
	static SSL_CTX *_context;

	/* The connection is a state machine:
	 * 1. Uninitialised: _tssl = NULL, _ssl = NULL
	 * 2. Connecting: _tssl = NULL, _ssl != NULL
	 * 3. Connected: _tssl != NULL, _ssl = NULL
	 */
	ThreadSSL *_tssl;
	SSL *_ssl;

	MessageBlock *_message;

	uint32_t _user_id;
	PermissionSet _permissions;

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

	uint32_t getUserId() const;
	void setUserId(uint32_t user_id);
	PermissionSet &permissions();
	const PermissionSet &permissions() const;

	static bool init();
};

#endif
