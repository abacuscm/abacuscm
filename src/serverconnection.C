#include "serverconnection.h"
#include "logger.h"
#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/rand.h>

using namespace std;

ServerConnection::ServerConnection() {
	_sock = -1;
	_ssl = NULL;
	_ctx = NULL;
}

ServerConnection::~ServerConnection() {
	if(_sock > 0)
		disconnect();
	if(_ctx) {
		SSL_CTX_free(_ctx);
		_ctx = NULL;
	}
}

bool ServerConnection::connect(string server, string service) {
	Config &config = Config::getConfig();
	
	if(_sock > 0) {
		log(LOG_ERR, "Already connected!");
		return false;
	}

	if(!_ctx) {
		_ctx = SSL_CTX_new(TLSv1_client_method());
		if(!_ctx) {
			log_ssl_errors("SSL_CTX_new");
			return false;
		}

		if(!SSL_CTX_load_verify_locations(_ctx,
					config["server"]["cacert"].c_str(), NULL)) {
			log_ssl_errors("SSL_CTX_load_verify_locations");
			return false;
		}

		SSL_CTX_set_verify(_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
	}

	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	addrinfo * res = NULL;
	addrinfo * i;

	if(getaddrinfo(server.c_str(), service.c_str(), &hints, &res) < 0) {
		lerror("getaddrinfo");
		return false;
	}

	for(i = res; i; i = i->ai_next) {
		_sock = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
		if(_sock < 0)
			lerror("socket");
		else // yes, this can go in the condition above but then we lose i.
			break;
	}

	if(_sock < 0) {
		freeaddrinfo(res);
		return false;
	}
	
	if(::connect(_sock, i->ai_addr, i->ai_addrlen) < 0) {
		lerror("connect");
		freeaddrinfo(res);
		return false;
	}
	freeaddrinfo(res);

	_ssl = SSL_new(_ctx);
	if(!_ssl) {
		log_ssl_errors("SSL_new");
		disconnect();
		return false;
	}

	if(!SSL_set_fd(_ssl, _sock)) {
		log_ssl_errors("SSL_set_fd");
		disconnect();
		return false;
	}

	if(SSL_connect(_ssl) < 1) {
		log_ssl_errors("SSL_connect");
		disconnect();
		return false;
	}

	log(LOG_DEBUG, "TODO:  Check servername against cn of certificate");

	return true;
}

bool ServerConnection::disconnect() {
	if(_sock < 0) {
		log(LOG_WARNING, "Attempting to disconnect non-connected socket.");
		return false;
	}
	if(_ssl) {
		SSL_shutdown(_ssl);
		SSL_free(_ssl);
		_ssl = NULL;
	}
	close(_sock);
	return true;
}

bool ServerConnection::auth(string username, string password) {
	NOT_IMPLEMENTED();
	return false;
}

bool ServerConnection::registerEventCallback(string event, EventCallback func, void *custom) {
	NOT_IMPLEMENTED();
	return false;
}

bool ServerConnection::deregisterEventCallback(string event, EventCallback func) {
	NOT_IMPLEMENTED();
	return false;
}

static void init() __attribute__((constructor));
static void init() {
	SSL_library_init();
	SSL_load_error_strings();
	RAND_load_file("/dev/urandom", 4096);
}
