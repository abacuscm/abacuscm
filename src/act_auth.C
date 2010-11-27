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
#include "dbcon.h"
#include "clienteventregistry.h"

class ActAuth : public ClientAction {
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

bool ActAuth::int_process(ClientConnection *cc, MessageBlock *mb) {
	DbCon *db = DbCon::getInstance();
	if(!db) {
		cc->sendError("Unable to connect to database.");
		return false;
	}

	uint32_t user_id;
	uint32_t user_type;
	int result = db->authenticate((*mb)["user"], (*mb)["pass"], &user_id, &user_type);
	db->release();db=NULL;

	if(result < 0)
		return cc->sendError("Database error.");
	else if(result == 0)
		return cc->sendError("Authentication failed.  Invalid username and/or password.");
	else {
		log(LOG_INFO, "User '%s' successfully logged in on client connection %p.", (*mb)["user"].c_str(), cc);
		cc->setProperty("user_id", user_id);
		cc->setProperty("user_type", user_type);
		ClientEventRegistry::getInstance().registerClient(cc);
		return cc->reportSuccess();
	}
}

static ActAuth _act_auth;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction(USER_TYPE_NONE, "auth", &_act_auth);
}
