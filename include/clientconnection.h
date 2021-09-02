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
#include <vector>
#include <pthread.h>

#include "socket.h"
#include "threadssl.h"
#include "permissions.h"
#include "queue.h"

#define CLIENT_BFR_SIZE			512

class MessageBlock;

class ClientConnection : public Socket {
private:
	static const SSL_METHOD *_method;
	static SSL_CTX *_context;

	/* Out-going data
	 */
	std::string _write_msg;      // data that is on its way out
	std::size_t _write_progress; // amount of _out_msg that has been sent
	Queue<std::string> _write_queue;  // messages waiting to be sent
	int _write_notify[2];        // pipe pair for waking up on data

	/* The connection is a state machine:
	 * 1. Uninitialised: _tssl = NULL, _ssl = NULL
	 * 2. Connecting: _tssl = NULL, _ssl != NULL
	 * 3. Connected: _tssl != NULL, _ssl = NULL
	 */
	ThreadSSL *_tssl;
	SSL *_ssl;

	MessageBlock *_message;

	uint32_t _user_id;
	uint32_t _group_id;
	PermissionSet _permissions;

	/**
	 * Attempts to open the SSL connection, but will not block. Returns:
	 * <0 on terminal failure
	 *  0 on success (connection open)
	 * >0 a pollfd events mask to wait for on the socket.
	 */
	short initiate_ssl();

	/**
	 * Attempts to read data from the connection, but will not block. Returns:
	 * <0 on failure (kill off the connection)
	 * >0 a pollfd events mask to wait for on the socket.
	 */
	short read_data();

	/**
	 * Attempts to write data to the connection, but will not block. Returns:
	 * <0 on failure (kill off the connection)
	 *  0 to wait for more data to be added to the queue
	 * >0 a pollfd events mask to wait for on the socket.
	 */
	short write_data();
public:
	ClientConnection(int sockfd);
	virtual ~ClientConnection();

	virtual std::vector<pollfd> int_process();

	// Needs to be implemented since we inherit from Socket, but will never
	// be called since we overload int_process.
	virtual short socket_process() { return 0; }

	void sendMessageBlock(const MessageBlock *mb);

	uint32_t getUserId() const;
	void setUserId(uint32_t user_id);
	uint32_t getGroupId() const;
	void setGroupId(uint32_t group_id);
	PermissionSet &permissions();
	const PermissionSet &permissions() const;

	static bool init();
};

#endif
