/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include "clientlistener.h"
#include "clientconnection.h"
#include "logger.h"
#include "acmconfig.h"

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

	int opt = 1;
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		lerror("setsockopt");

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atol(Config::getConfig()["clientlistener"]["port"].c_str()));
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

	if(fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
		lerror("fcntl");
		log(LOG_INFO, "Continueing anyway ...");
	}

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

	if(fcntl(res, F_SETFL, O_NONBLOCK) < 0) {
		lerror("fcntl");
		log(LOG_INFO, "Continueing anyway ...");
	}

	ClientConnection *client = new ClientConnection(res);
	_pool->locked_insert(client);

	log(LOG_INFO, "Accepted new connection from %s.", inet_ntoa(addr.sin_addr));
	return true;
}
