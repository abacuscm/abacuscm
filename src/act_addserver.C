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
#include "logger.h"
#include "message_createserver.h"
#include "server.h"
#include "misc.h"
#include <memory>

using namespace std;

class ActAddServer : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

auto_ptr<MessageBlock> ActAddServer::int_process(ClientConnection* cc, const MessageBlock* mb) {
	if(Server::getId() != 1)
		return MessageBlock::error("Can only add servers from the master server");

	uint32_t user_id = cc->getUserId();

	string servername = (*mb)["servername"];
	if(servername == "")
		return MessageBlock::error("Cannot specify an empty server name");

	log(LOG_NOTICE, "User %u requested addition of server '%s'", user_id,
			servername.c_str());

	uint32_t server_id = Server::server_id(servername);

	if(server_id)
		return MessageBlock::error("Servername is already in use!");

	server_id = Server::nextServerId();
	if(server_id == ~0U)
		return MessageBlock::error("Unable to determine next server_id");

	Message_CreateServer *msg = new Message_CreateServer(servername, server_id);

	MessageHeaders::const_iterator i;
	for(i = mb->begin(); i != mb->end(); ++i) {
		if(i->first != "servername" && i->first != "content-length")
			msg->addAttribute(i->first, i->second);
	}

	return triggerMessage(cc, msg);
}

static ActAddServer _act_addserver;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("addserver", PERMISSION_SERVER_ADMIN, &_act_addserver);
}
