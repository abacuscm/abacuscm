#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "clientlistener.h"
#include "logger.h"

ClientListener::ClientListener() {
	_pool = NULL;
}

ClientListener::~ClientListener() {
}

bool ClientListener::init(SocketPool *pool) {
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;

	
	if(sock < 0) {
		lerror("socket");
		return false;
	}

	if(bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
		lerror("bind");
		goto err;
	}

	if(listen(sock, 128) < 0) {
		lerror("listen");
		goto err;
	}

	sockfd() = sock;

	_pool = pool;
	return true;

err:
	close(sock);
	return false;
}

bool ClientListener::process() {
	sockaddr_in addr;
	socklen_t size = sizeof(addr);
	int res = accept(sockfd(), (sockaddr*)&addr, &size);
	if(res < 0)
		return true;

	log(LOG_INFO, "Accepted new connection from %s.", inet_ntoa(addr.sin_addr));
	return true;
}
