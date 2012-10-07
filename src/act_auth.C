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
#include "permissionmap.h"
#include "markers.h"

#include <memory>

using namespace std;

class ActAuth : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

auto_ptr<MessageBlock> ActAuth::int_process(ClientConnection *cc, const MessageBlock *mb) {
	DbCon *db = DbCon::getInstance();
	if(!db) {
		return MessageBlock::error("Unable to connect to database.");
	}

	uint32_t user_id;
	uint32_t user_type;
	uint32_t group_id;
	int result = db->authenticate((*mb)["user"], (*mb)["pass"], &user_id, &user_type, &group_id);
	db->release();db=NULL;

	if(result < 0)
		return MessageBlock::error("Database error.");
	else if(result == 0)
		return MessageBlock::error("Authentication failed.  Invalid username and/or password.");
	else {
		if (cc->getUserId() != 0) {
			// Client was already logged in - fake disconnection
			ClientEventRegistry::getInstance().deregisterClient(cc);
			Markers::getInstance().preemptMarker(cc);
		}
		log(LOG_INFO, "User '%s' successfully logged in on client connection %p.", (*mb)["user"].c_str(), cc);
		cc->setUserId(user_id);
		cc->setGroupId(group_id);
		cc->permissions() = PermissionMap::getInstance()->getPermissions(static_cast<UserType>(user_type));
		ClientEventRegistry::getInstance().registerClient(cc);
		auto_ptr<MessageBlock> ret = MessageBlock::ok();
		(*ret)["user"] = (*mb)["user"];
		return ret;
	}
}

static ActAuth _act_auth;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("auth", PermissionTest::ANY, &_act_auth);
}
