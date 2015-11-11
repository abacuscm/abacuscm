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
#include "dbcon.h"
#include "misc.h"

#include <memory>

using namespace std;

class ServerListAct : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

auto_ptr<MessageBlock> ServerListAct::int_process(ClientConnection *, const MessageBlock *) {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return MessageBlock::error("Error connecting to database");

	ServerList list = db->getServers();
	db->release();db=NULL;

	auto_ptr<MessageBlock> mb(MessageBlock::ok());
	int c = 0;
	for(ServerList::iterator i = list.begin(); i != list.end(); ++i, ++c) {
		(*mb)["server" + to_string(c)] = i->second;
	}

	return mb;
}

static ServerListAct _act_servlist;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("getserverlist", PERMISSION_SERVER_ADMIN, &_act_servlist);
}
