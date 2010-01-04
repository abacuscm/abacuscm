/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>

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
	struct addrinfo hints;
	struct addrinfo *adinf = NULL;
	struct addrinfo *i;

	int sock = 0;

	std::string bind = Config::getConfig()["clientlistener"]["bind"];
	std::string port = Config::getConfig()["clientlistener"]["port"];

	if (bind == "")
		bind = "any";

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(bind.c_str(), port.c_str(), &hints, &adinf)) {
		lerror("getaddrinfo");
		return false;
	}


	for (i = adinf; i && sock <= 0; i = i->ai_next) {
		sock = socket(i->ai_family, i->ai_socktype, i->ai_protocol);

		if (sock < 0) {
			lerror("socket");
			continue;
		}

		int opt = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
			lerror("setsockopt");


		if (::bind(sock, i->ai_addr, i->ai_addrlen) < 0) {
			lerror("bind");
			close(sock);
			sock = 0;
			continue;
		}

		if (listen(sock, 128) < 0) {
			lerror("listen");
			close(sock);
			sock = 0;
			continue;
		}

		sockfd() = sock;

		if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
			lerror("fcntl");
			log(LOG_INFO, "Continueing anyway ...");
		}
	}

	freeaddrinfo(adinf);

	if (sock <= 0) {
		log(LOG_CRIT, "Unable to bind client listener to any sockets, bailing out.");
		return false;
	}

	_pool = pool;
	return true;
}

bool ClientListener::process() {
	struct sockaddr addr;
	socklen_t size = sizeof(addr);
	char host[48];
	char port[7];

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

	if (getnameinfo(&addr, size, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST) == 0) {
		log(LOG_INFO, "Accepted new connection from %s:%s.", host, port);
	} else {
		log(LOG_WARNING, "Accepted new connection, but unable to figure out remote host address.");
	}

	return true;
}
