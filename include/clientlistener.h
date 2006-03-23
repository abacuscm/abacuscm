/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __CLIENTLISTENER_H__
#define __CLIENTLISTENER_H__

#include "socket.h"

#include <set>

typedef std::set<Socket*> SocketPool;

class ClientListener : public Socket {
private:
	SocketPool* _pool;
public:
	ClientListener();
	virtual ~ClientListener();

	bool init(SocketPool *pool);

	virtual bool process();
};

#endif
