/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include <vector>
#include <unistd.h>
#include "socket.h"

using namespace std;

Socket::Socket() {
	_sock = -1;
}

Socket::~Socket() {
	if(_sock >= 0)
		close(_sock);
}

void Socket::initialise(int sock, short events) {
	_sock = sock;
	vector<pollfd> fdevents;
	pollfd cur;
	cur.fd = sock;
	cur.events = events;
	cur.revents = 0;
	fdevents.push_back(cur);
	Waitable::initialise(fdevents);
}

vector<pollfd> Socket::int_process() {
	vector<pollfd> ans;
	short events = socket_process();
	if (events != 0) {
		pollfd cur;
		cur.fd = getSock();
		cur.events = events;
		cur.revents = 0;
		ans.push_back(cur);
	}
	return ans;
}
