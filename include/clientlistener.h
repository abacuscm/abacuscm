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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "socket.h"

class ClientListener : public Socket {
private:
	WaitableSet* _pool;
public:
	ClientListener();
	virtual ~ClientListener();

	bool init(WaitableSet *pool);

	virtual short socket_process();
};

#endif
