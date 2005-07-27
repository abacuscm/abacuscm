#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "clientlistener.h"
#include "clientconnection.h"
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

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(7386);
	addr.sin_addr.s_addr = INADDR_ANY;

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
	if(res < 0) {
		lerror("accept");
		return true;
	}

	ClientConnection *client = new ClientConnection(res);
	_pool->insert(client);

	log(LOG_INFO, "Accepted new connection from %s.", inet_ntoa(addr.sin_addr));
	return true;
}
