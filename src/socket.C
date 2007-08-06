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
#include <unistd.h>

#include "socket.h"
#include "logger.h"

Socket::Socket() {
	_sock = -1;
}

Socket::~Socket() {
	if(_sock > 0)
		close(_sock);
}

void Socket::addToSet(int *n, fd_set *set) {
	if(_sock < 0) {
		log(LOG_WARNING, "Attempting to add a negative socket number to the select() set.");
		return;
	}
	if(_sock >= *n)
		*n = _sock + 1;
	FD_SET(_sock, set);
}

bool Socket::isInSet(fd_set *set) {
	return FD_ISSET(sockfd(), set);
}
