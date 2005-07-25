#include "socket.h"
#include "logger.h"

Socket::Socket() {
	_sock = -1;
}

Socket::~Socket() {
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
