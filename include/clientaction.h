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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <map>
#include <string>

#include "queue.h"
#include "permissions.h"

class ClientConnection;
class MessageBlock;
class Message;

class ClientAction {
private:
	struct Info
	{
		PermissionTest pt;  // permissions required to use this action
		ClientAction *action;

		Info() : pt(), action(NULL) {}
		Info(PermissionTest pt, ClientAction *action) : pt(pt), action(action) {}
	};

	static Queue<Message*> *_message_queue;
	static std::map<std::string, Info> _actionmap;

protected:
	void triggerMessage(ClientConnection *cc, Message*);
	virtual void int_process(ClientConnection *cc, MessageBlock *mb) = 0;
public:
	ClientAction();
	virtual ~ClientAction();

	static void setMessageQueue(Queue<Message*> *message_queue);
	static bool registerAction(const std::string &name, const PermissionTest &pt, ClientAction *ca);
	static void process(ClientConnection *cc, MessageBlock *mb);
};

#endif
