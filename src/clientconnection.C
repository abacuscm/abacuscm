/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include <openssl/rand.h>
#include <openssl/ssl.h>

#include <cassert>
#include <new>
#include <fcntl.h>
#include "clientconnection.h"
#include "logger.h"
#include "acmconfig.h"
#include "messageblock.h"
#include "clientaction.h"
#include "clienteventregistry.h"
#include "markers.h"
#include "threadssl.h"

using namespace std;

/* See comment in header file */
#if OPENSSL_VERSION_NUMBER < 0x10000000
SSL_METHOD *ClientConnection::_method = NULL;
#else
const SSL_METHOD *ClientConnection::_method = NULL;
#endif
SSL_CTX *ClientConnection::_context = NULL;

ClientConnection::ClientConnection(int sock) {
	_tssl = NULL;
	_ssl = NULL;
	_message = NULL;
	_user_id = 0;
	// TODO: error checking
	pipe(_write_notify);
	fcntl(_write_notify[0], F_SETFL, O_NONBLOCK);
	fcntl(_write_notify[1], F_SETFL, O_NONBLOCK);
	_write_progress = 0;
	// This might be more generous than necessary, but if we get one early
	// wakeup it won't hurt.
	initialise(sock, POLLIN | POLLOUT);
}

ClientConnection::~ClientConnection() {
	ClientEventRegistry::getInstance().deregisterClient(this);
	Markers::getInstance().preemptMarker(this);

	if(_message)
		delete _message;

	if(_ssl) {
		log(LOG_DEBUG, "Shutting down (incomplete) connection %p", this);
		SSL_shutdown(_ssl);
		SSL_free(_ssl);
		_ssl = NULL;
	}
	if(_tssl) {
		log(LOG_DEBUG, "Shutting down connection %p", this);
		delete _tssl;
		_tssl = NULL;
	}

	close(_write_notify[0]);
	close(_write_notify[1]);
}

short ClientConnection::initiate_ssl() {
	assert(_tssl == NULL);
	if(!_ssl) {
		_ssl = SSL_new(_context);
		if(!_ssl) {
			log_ssl_errors("SSL_new");
			goto err;
		}

		if(!SSL_set_fd(_ssl, getSock())) {
			log_ssl_errors("SSL_set_fd");
			goto err;
		}
	}


	switch(unsigned err = SSL_get_error(_ssl, SSL_accept(_ssl))) {
		case SSL_ERROR_NONE:
			_tssl = new(nothrow) ThreadSSL(_ssl);
			if (_tssl == NULL) {
				log(LOG_ERR, "OOM allocating ThreadSSL");
				goto err;
			}
			_ssl = NULL;
			// Success
			return 0;
		case SSL_ERROR_WANT_READ:
			return POLLIN;
		case SSL_ERROR_WANT_WRITE:
			return POLLOUT;
		default:
			log_ssl_errors_with_err("SSL_accept", err);
			goto err;
	}

err:
	if(_ssl) {
		SSL_free(_ssl);
		_ssl = NULL;
	}
	if (_tssl) {
		delete _tssl;
		_tssl = NULL;
	}

	return -1;
}

short ClientConnection::read_data() {
	char buffer[CLIENT_BFR_SIZE];
	ThreadSSL::Status status;
	while(0 < (status = _tssl->read(buffer, CLIENT_BFR_SIZE, ThreadSSL::NONBLOCK)).processed) {
		char *pos = buffer;
		int res = status.processed;
		while(res) {
			if(!_message)
				_message = new MessageBlock;

			int res2 = _message->addBytes(pos, res);
			if(res2 < 0)
				return -1;
			if(res2 > 0) {
				ClientAction::process(this, _message);
				delete _message;
				_message = NULL;
				pos += res2;
				res -= res2;
			} else
				res = 0;
		}
	}

	switch(status.err) {
		case SSL_ERROR_WANT_READ:
			return POLLIN;
		case SSL_ERROR_WANT_WRITE:
			return POLLOUT;
		case SSL_ERROR_NONE:
			// Can this happen? Implies no data was actually read.
			// Fall through for now
		case SSL_ERROR_ZERO_RETURN:
			// connection shut down
			return -1;
		default:
			log_ssl_errors_with_err("SSL_read", status.err);
			return -1;
	}
}

short ClientConnection::write_data() {
	char dummy[64];
	// Drain the wakeup queue
	while (read(_write_notify[0], dummy, sizeof(dummy)) > 0) {
		// Do nothing
	}

	while (true) {
		if (_write_msg.empty()) {
			if (_write_queue.empty()) {
				return 0; // caller must wait until more is enqueued
			}
			_write_msg = _write_queue.dequeue();
		}
		ThreadSSL::Status status;
		size_t remain = _write_msg.size() - _write_progress;
		assert(remain > 0);
		status = _tssl->write(_write_msg.data() + _write_progress, remain, ThreadSSL::NONBLOCK);
		if (status.processed == 0) {
			switch (status.err) {
			case SSL_ERROR_WANT_READ:
				return POLLIN;
			case SSL_ERROR_WANT_WRITE:
				return POLLOUT;
			case SSL_ERROR_NONE:
				// Can this happen? Implies no data was actually read.
				// Fall through for now
			case SSL_ERROR_ZERO_RETURN:
				// connection shut down
				return -1;
			default:
				log_ssl_errors_with_err("SSL_write", status.err);
				return -1;
			}
		}
		_write_progress += status.processed;
		if (_write_progress == _write_msg.size()) {
			_write_progress = 0;
			_write_msg = "";
		}
	}
}

vector<pollfd> ClientConnection::int_process() {
	vector<pollfd> fds;
	if(!_tssl) {
		short status = initiate_ssl();
		if (status == -1)
			return fds; // error
		else if (status > 0) {
			fds.push_back(pollfd());
			fds.back().fd = getSock();
			fds.back().events = status;
			return fds;
		}
	}

	/* If we get here, the connection was established */

	short read_status = read_data();
	if (read_status < 0)
		return fds; // error

	short write_status = write_data();
	if (write_status < 0)
		return fds; // error

	// TODO: check for SSL_pending after the write and go back to reading?

	if (read_status | write_status) {
		fds.push_back(pollfd());
		fds.back().fd = getSock();
		fds.back().events = read_status | write_status;
	}
	if (write_status == 0) { // asked to wait for more incoming data
		fds.push_back(pollfd());
		fds.back().fd = _write_notify[0];
		fds.back().events = POLLIN;
	}
	return fds;
}

void ClientConnection::sendError(const std::string& message) {
	MessageBlock mb("err");
	mb["msg"] = message;
	sendMessageBlock(&mb);
}

void ClientConnection::reportSuccess() {
	MessageBlock mb("ok");
	sendMessageBlock(&mb);
}

void ClientConnection::sendMessageBlock(const MessageBlock *mb) {
	string raw = mb->getRaw();
	if (!raw.empty()) {
		char dummy = 0;
		_write_queue.enqueue(raw);
		// Wake up anyone who might be waiting for data
		write(_write_notify[1], &dummy, 1);
	}
}

bool ClientConnection::init() {
	Config& config = Config::getConfig();

	log(LOG_DEBUG, "Loaded %d bytes from /dev/urandom", RAND_load_file("/dev/urandom", 4096));

	_method = TLSv1_server_method();
	if(!_method) {
		log_ssl_errors("SSL_server_method");
		goto err;
	}

	/* OpenSSL versions prior to 1.0.0 don't take a const parameter, so we have
	 * to explicitly make it unconst. Simply changing _method to be non-const
	 * doesn't work, because in 1.0.0 the return from TLSv1_server_method is
	 * const.
	 */
	_context = SSL_CTX_new(_method);
	if(!_context) {
		log_ssl_errors("SSL_CTX_new");
		goto err;
	}

	if(!SSL_CTX_use_certificate_file(_context,
				config["clientlistener"]["x509"].c_str(), SSL_FILETYPE_PEM)) {
		log_ssl_errors("SSL_CTX_use_certificate_file");
		goto err;
	}

	if(!SSL_CTX_use_PrivateKey_file(_context,
				config["clientlistener"]["private"].c_str(),
				SSL_FILETYPE_PEM)) {
		log_ssl_errors("SSL_CTX_use_PrivateKey_file");
		goto err;
	}

	if(!SSL_CTX_check_private_key(_context)) {
		log_ssl_errors("SSL_CTX_check_private_key");
		goto err;
	}

	if(!SSL_CTX_set_cipher_list(_context,
				config["clientlistener"]["ciphers"].c_str())) {
		log_ssl_errors("SSL_CTX_set_cipher_list");
		goto err;
	}

	SSL_CTX_set_options(_context, SSL_OP_NO_SSLv2 | SSL_OP_SINGLE_DH_USE);

	return true;
err:
	if(_context) {
		SSL_CTX_free(_context);
		_context = NULL;
	}
	if(_method)
		_method = NULL;
	return false;
}

void ClientConnection::setUserId(uint32_t id) {
	_user_id = id;
}

uint32_t ClientConnection::getUserId() const {
	return _user_id;
}

PermissionSet &ClientConnection::permissions() {
	return _permissions;
}

const PermissionSet &ClientConnection::permissions() const {
	return _permissions;
}
