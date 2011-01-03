/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "message.h"
#include "logger.h"
#include "server.h"
#include "misc.h"
#include "permissions.h"

std::map<std::string, ClientAction::Info> ClientAction::_actionmap;
Queue<Message*> *ClientAction::_message_queue;

ClientAction::ClientAction() {
}

ClientAction::~ClientAction() {
}

bool ClientAction::registerAction(const std::string &action, const PermissionTest &pt, ClientAction *ca) {
	if(_actionmap.find(action) != _actionmap.end())
		log(LOG_WARNING, "Overriding action handler for action '%s'!", action.c_str());

	log(LOG_INFO, "Registering action '%s'", action.c_str());
	_actionmap[action] = Info(pt, ca);
	return true;
}

void ClientAction::triggerMessage(ClientConnection *cc, Message *mb) {
	if(mb->makeMessage()) {
		_message_queue->enqueue(mb);
		cc->reportSuccess();
	} else {
		delete mb;
		cc->sendError("Internal error creating message.  This is indicative of a bug.");
	}
}

void ClientAction::process(ClientConnection *cc, MessageBlock *mb) {
	if(!Server::getId())
		cc->sendError("Server has not yet been initialised - please try again later");

	std::string action = mb->action();

	std::map<std::string, Info>::const_iterator it = _actionmap.find(action);
	if (it == _actionmap.end())
	{
		uint32_t user_id = cc->getUserId();
		log(LOG_NOTICE, "Unknown action '%s' encountered on ClientConnection for user %u.", action.c_str(), user_id);
		cc->sendError("No such action");
	}
	else if (!it->second.pt.matches(cc->permissions()))
	{
		uint32_t user_id = cc->getUserId();
		log(LOG_NOTICE, "Denied action '%s' for user %u.", action.c_str(), user_id);
		cc->sendError("Action not permitted");
	}
	else
	{
		it->second.action->int_process(cc, mb);
	}
}

void ClientAction::setMessageQueue(Queue<Message*> *message_queue) {
	_message_queue = message_queue;
}
