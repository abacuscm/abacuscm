#include "serverconnection.h"
#include "logger.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

using namespace std;

ServerConnection::ServerConnection() {
	_sock = -1;
	_ssl = NULL;
}

ServerConnection::~ServerConnection() {
	if(_sock > 0)
		disconnect();
}

bool ServerConnection::connect(string server, int port) {
	if(_sock > 0) {
		log(LOG_ERR, "Already connected!");
		return false;
	}

	struct hostent * host = gethostbyname(server.c_str());
	if(!host) {
		log(LOG_ERR, "Unable to retrieve server address");
		return false;
	}

	_sock = socket(host->h_addrtype, SOCK_STREAM, 0);
	if(_sock < 0) {
		lerror("socket");
		return false;
	}

	NOT_IMPLEMENTED();
	return false;
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
