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

bool ClientAction::registerAction(const std::string &action, const PermissionSet &ps, ClientAction *ca) {
	if(_actionmap.find(action) != _actionmap.end())
		log(LOG_WARNING, "Overriding action handler for action '%s'!", action.c_str());

	log(LOG_INFO, "Registering action '%s'", action.c_str());
	_actionmap[action] = Info(ps, ca);
	return true;
}

bool ClientAction::triggerMessage(ClientConnection *cc, Message *mb) {
	if(mb->makeMessage()) {
		_message_queue->enqueue(mb);
		return cc->reportSuccess();
	} else {
		delete mb;
		return cc->sendError("Internal error creating message.  This is indicative of a bug.");
	}
}

bool ClientAction::process(ClientConnection *cc, MessageBlock *mb) {
	bool status = true;
	if(!Server::getId())
		return cc->sendError("Server has not yet been initialised - please try again later");

	std::string action = mb->action();

	std::map<std::string, Info>::const_iterator it = _actionmap.find(action);
	if (it == _actionmap.end())
	{
		uint32_t user_type = cc->getProperty("user_type");
		uint32_t user_id = cc->getProperty("user_id");
		log(LOG_NOTICE, "Unknown action '%s' encountered on ClientConnection for user %u of type %u.", action.c_str(), user_id, user_type);
		cc->sendError("No such action");
	}
	else if (!Permissions::getInstance()->hasPermission(cc, it->second.ps))
	{
		uint32_t user_type = cc->getProperty("user_type");
		uint32_t user_id = cc->getProperty("user_id");
		log(LOG_NOTICE, "Denied action '%s' for user %u of type %u.", action.c_str(), user_id, user_type);
		cc->sendError("Action not permitted");
	}
	else
	{
		status = it->second.action->int_process(cc, mb);
	}
	return status;
}

void ClientAction::setMessageQueue(Queue<Message*> *message_queue) {
	_message_queue = message_queue;
}
