/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <sys/types.h>

class Socket {
private:
	int _sock;
protected:
	int& sockfd() { return _sock; };
public:
	Socket();
	virtual ~Socket();

	/**
	 * returns true or false.
	 * true:   re-add to the idle_pool.
	 * false:  delete object.
	 */
	virtual bool process() = 0;

	void addToSet(int *n, fd_set *set);
	bool isInSet(fd_set *set);
};

#endif
