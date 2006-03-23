/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __NETLIB_H__
#define __NETLIB_H__

#include <stdint.h>

class Message;

class PeerMessenger {
private:
	static PeerMessenger *_messenger;
public:
	PeerMessenger();
	virtual ~PeerMessenger();

	virtual bool initialise() = 0;
	virtual void deinitialise() = 0;
	virtual bool sendMessage(uint32_t server_id, const Message * message) = 0;
	virtual Message* getMessage() = 0;
	virtual void sendAck(uint32_t server_id, uint32_t message_id) = 0;

	static void setMessenger(PeerMessenger *messenger);
	static PeerMessenger * getMessenger();
};

#endif
