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

#include <sstream>

class ServerListAct : public ClientAction {
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

bool ServerListAct::int_process(ClientConnection *cc, MessageBlock *) {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Error connecting to database");

	ServerList list = db->getServers();
	db->release();db=NULL;

	MessageBlock mb("ok");
	int c = 0;
	for(ServerList::iterator i = list.begin(); i != list.end(); ++i, ++c) {
		std::ostringstream t;
		t << "server" << c;
		mb[t.str()] = i->second;
	}

	return cc->sendMessageBlock(&mb);
}

static ServerListAct _act_servlist;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("getserverlist", PERMISSION_SERVER_ADMIN, &_act_servlist);
}
