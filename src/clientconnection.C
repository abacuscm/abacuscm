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
#include "clientconnection.h"
#include "logger.h"
#include "acmconfig.h"
#include "messageblock.h"
#include "clientaction.h"
#include "clienteventregistry.h"
#include "markers.h"
#include "threadssl.h"

using namespace std;

SSL_METHOD *ClientConnection::_method = NULL;
SSL_CTX *ClientConnection::_context = NULL;

ClientConnection::ClientConnection(int sock) {
	sockfd() = sock;
	_tssl = NULL;
	_ssl = NULL;
	_message = NULL;
	pthread_mutex_init(&_write_lock, NULL);
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

	pthread_mutex_destroy(&_write_lock);
}

bool ClientConnection::initiate_ssl() {
	assert(_tssl == NULL);
	if(!_ssl) {
		_ssl = SSL_new(_context);
		if(!_ssl) {
			log_ssl_errors("SSL_new");
			goto err;
		}

		if(!SSL_set_fd(_ssl, sockfd())) {
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
			break;
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			break;
		default:
			log_ssl_errors_with_err("SSL_accept", err);
			goto err;
	}

	return true;
err:
	if(_ssl) {
		SSL_free(_ssl);
		_ssl = NULL;
	}
	if (_tssl) {
		delete _tssl;
		_tssl = NULL;
	}

	return false;
}

bool ClientConnection::process_data() {
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
				return false;
			if(res2 > 0) {
				bool proc_res = ClientAction::process(this, _message);
				delete _message;
				_message = NULL;
				if(!proc_res)
					return false;
				pos += res2;
				res -= res2;
			} else
				res = 0;
		}
	}

	switch(status.err) {
		case SSL_ERROR_NONE:
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			return true;
		case SSL_ERROR_ZERO_RETURN:
			// connection shut down
			return false;
		default:
			log_ssl_errors_with_err("SSL_read", status.err);
			return false;
	}
}

bool ClientConnection::process() {
	if(!_tssl)
		return initiate_ssl();
	else
		return process_data();

	return false;
}

bool ClientConnection::sendError(const std::string& message) {
	MessageBlock mb("err");
	mb["msg"] = message;
	return sendMessageBlock(&mb);
}

bool ClientConnection::reportSuccess() {
	MessageBlock mb("ok");
	return sendMessageBlock(&mb);
}

bool ClientConnection::sendMessageBlock(const MessageBlock *mb) {
	/* Note: the ThreadSSL lock only protects readers and writers from each
	 * other. We need need the _write_lock to prevent the data from multiple
	 * concurrent callers to this function from becoming interleaved.
	 */
	pthread_mutex_lock(&_write_lock);
	bool res = mb->writeToSSL(_tssl);
	pthread_mutex_unlock(&_write_lock);
	if(!res)
		log(LOG_NOTICE, "Failed to write MessageBlock to client - not sure what this means though");
	return res;
}

bool ClientConnection::init() {
	Config& config = Config::getConfig();

	log(LOG_DEBUG, "Loaded %d bytes from /dev/urandom", RAND_load_file("/dev/urandom", 4096));

	_method = TLSv1_server_method();
	if(!_method) {
		log_ssl_errors("SSL_server_method");
		goto err;
	}

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
