#include <openssl/rand.h>
#include <openssl/ssl.h>

#include "clientconnection.h"
#include "logger.h"
#include "config.h"

SSL_METHOD *ClientConnection::_method = NULL;
SSL_CTX *ClientConnection::_context = NULL;

ClientConnection::ClientConnection(int sock) {
	sockfd() = sock;
	_ssl = NULL;
}

ClientConnection::~ClientConnection() {
	if(_ssl) {
		SSL_shutdown(_ssl);
		SSL_free(_ssl);
	}
}

bool ClientConnection::initiate_ssl() {
	_ssl = SSL_new(_context);
	if(!_ssl) {
		log_ssl_errors("SSL_new");
		goto err;
	}

	if(!SSL_set_fd(_ssl, sockfd()) < 0) {
		log_ssl_errors("SSL_set_fd");
		goto err;
	}
	
	if(SSL_accept(_ssl) <= 0) {
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

bool ClientConnection::process() {
	if(!_ssl)
		return initiate_ssl();
	
	NOT_IMPLEMENTED();
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
