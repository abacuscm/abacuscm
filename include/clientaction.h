/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __CLIENTACTION_H__
#define __CLIENTACTION_H__

#include <map>
#include <string>

#include "queue.h"

class ClientConnection;
class MessageBlock;
class Message;

class ClientAction {
private:
	static Queue<Message*> *_message_queue;
	static std::map<int, std::map<std::string, ClientAction*> > actionmap;
protected:
	bool triggerMessage(ClientConnection *cc, Message*);
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb) = 0;
public:
	ClientAction();
	virtual ~ClientAction();

	static void setMessageQueue(Queue<Message*> *message_queue);
	static bool registerAction(int user_type, std::string action, ClientAction *ca);
	static bool process(ClientConnection *cc, MessageBlock *mb);
};

#endif
