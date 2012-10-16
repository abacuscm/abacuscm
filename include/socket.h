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
#include "waitable.h"

class Socket : public Waitable {
private:
	int _sock;
protected:
	int getSock() const { return _sock; }
public:
	Socket();
	virtual ~Socket();

	void initialise(int sock, short revents);
	virtual std::vector<pollfd> int_process();

	// Subclases can implement this instead of int_process. It will turn
	// the result into a pollfd using the return value as the events member,
	// with _sock being the file descriptor.
	virtual short socket_process() = 0;
};

#endif
