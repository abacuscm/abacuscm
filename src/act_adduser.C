/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "logger.h"
#include "server.h"
#include "message_createuser.h"
#include "usersupportmodule.h"

#include <map>

using namespace std;

class ActAddUser : public ClientAction {
private:
	map<string, uint32_t> _typemap;

protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
public:
	ActAddUser();
};

ActAddUser::ActAddUser() {
	_typemap["admin"] = USER_TYPE_ADMIN;
	_typemap["judge"] = USER_TYPE_JUDGE;
	_typemap["contestant"] = USER_TYPE_CONTESTANT;
	_typemap["marker"] = USER_TYPE_MARKER;
}

bool ActAddUser::int_process(ClientConnection *cc, MessageBlock *mb) {
	UserSupportModule *usm = getUserSupportModule();
	uint32_t user_id = cc->getProperty("user_id");
	string new_username = (*mb)["username"];
	string new_friendlyname = (*mb)["friendlyname"];
	string new_passwd = (*mb)["passwd"];
	uint32_t new_type = _typemap[(*mb)["type"]];

	if (!usm)
		return cc->sendError("UserSupportModule not available.");

	if(!new_type)
		return cc->sendError("Invalid user type " + (*mb)["type"]);

	if(new_username == "")
		return cc->sendError("Cannot have a blank username");

	if(new_passwd == "")
		return cc->sendError("Cannot set a blank password");

	if ((new_passwd = usm->hashpw(new_username, new_passwd)) == "")
		return cc->sendError("Password hashing error");

	if (new_friendlyname == "")
		new_friendlyname = new_username;

	uint32_t tmp_user_id = usm->user_id(new_username);

	if(tmp_user_id)
		return cc->sendError("Username is already in use");

	uint32_t new_id = usm->nextId();
	if(new_id == ~0U)
		return cc->sendError("Unable to determine next usable user_id");

	return triggerMessage(cc, new Message_CreateUser(new_username, new_friendlyname,
				new_passwd, new_id, new_type, user_id));
}

static ActAddUser _act_adduser;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "adduser", &_act_adduser);
}
