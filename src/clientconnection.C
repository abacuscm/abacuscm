#include <openssl/rand.h>
#include <openssl/ssl.h>

#include "clientconnection.h"
#include "logger.h"
#include "config.h"
#include "messageblock.h"

SSL_METHOD *ClientConnection::_method = NULL;
SSL_CTX *ClientConnection::_context = NULL;

ClientConnection::ClientConnection(int sock) {
	sockfd() = sock;
	_ssl = NULL;
	_message = NULL;
}

ClientConnection::~ClientConnection() {
	if(_ssl) {
		log(LOG_DEBUG, "Shutting down connection");
		SSL_shutdown(_ssl);
		SSL_free(_ssl);
	}
}

bool ClientConnection::initiate_ssl() {
	if(!_ssl) {
		_ssl = SSL_new(_context);
		if(!_ssl) {
			log_ssl_errors("SSL_new");
			goto err;
		}

		if(!SSL_set_fd(_ssl, sockfd()) < 0) {
			log_ssl_errors("SSL_set_fd");
			goto err;
		}
	}
	
	switch(SSL_get_error(_ssl, SSL_accept(_ssl))) {
		case SSL_ERROR_NONE:
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			break;
		default:
			log_ssl_errors("SSL_accept");
			goto err;
	}

	return true;
err:
	if(_ssl) {
		SSL_free(_ssl);
		_ssl = NULL;
	}
	
	return false;
		
}

bool ClientConnection::process_data() {
	char buffer[CLIENT_BFR_SIZE];
	int res;
	while(0 < (res = SSL_read(_ssl, buffer, CLIENT_BFR_SIZE))) {
		char *pos = buffer;
		while(res) {
			if(!_message)
				_message = new MessageBlock;

			int res2 = _message->addBytes(pos, res);
			log(LOG_DEBUG, "addBytes() returned %d", res2);
			if(res2 < 0)
				return false;
			if(res2 > 0) {
				_message->dump();
				delete _message;
				_message = NULL;
				pos += res2;
				res -= res2;
			} else
				res = 0;
		}
	}

	switch(SSL_get_error(_ssl, res)) {
		case SSL_ERROR_NONE:
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			return true;
		case SSL_ERROR_ZERO_RETURN:
			// clean shut down.
			return false;
		default:
			log_ssl_errors("SSL_read");
			return false;
	}
}

bool ClientConnection::process() {
	if(!_ssl || !SSL_is_init_finished(_ssl))
		return initiate_ssl();
	else
		return process_data();
	
	return false;
}

int ClientConnection::getUserType() {
	return USER_TYPE_NONE;
}

bool ClientConnection::init() {
	Config& config = Config::getConfig();

	SSL_library_init();
	SSL_load_error_strings();

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
