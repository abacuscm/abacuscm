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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <sys/types.h>

#include <set>

class Socket;

class SocketPool : public std::set<Socket*> {
private:
	pthread_mutex_t _lock;
public:
	SocketPool();
	~SocketPool();

	void lock();
	void unlock();

	void locked_insert(Socket* s) { lock(); insert(s); unlock(); }
};

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
