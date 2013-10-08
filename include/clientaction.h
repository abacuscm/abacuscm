/**
 * Copyright (c) 2005 - 2006, 2011 Kroon Infomation Systems,
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
#include <memory>

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
	/* Enqueues a server message, waits for it to be processed and then returns
	 * a reply to send to the client - either the given reply on success or an
	 * error message on failure.
	 */
	std::auto_ptr<MessageBlock> triggerMessage(ClientConnection *cc, Message*msg, std::auto_ptr<MessageBlock> reply);
	/* As above, but on success sends an empty "ok" message
	 */
	std::auto_ptr<MessageBlock> triggerMessage(ClientConnection *cc, Message*msg);

	virtual std::auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb) = 0;
public:
	ClientAction();
	virtual ~ClientAction();

	static void setMessageQueue(Queue<Message*> *message_queue);
	static bool registerAction(const std::string &name, const PermissionTest &pt, ClientAction *ca);
	static void process(ClientConnection *cc, const MessageBlock *mb);
};

#endif
