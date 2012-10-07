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
#include "server.h"
#include "message_createuser.h"
#include "usersupportmodule.h"
#include "misc.h"
#include "hashpw.h"

#include <map>
#include <memory>

using namespace std;

class ActAddUser : public ClientAction {
private:
	map<string, uint32_t> _typemap;

protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
public:
	ActAddUser();
};

ActAddUser::ActAddUser() {
	_typemap["admin"] = USER_TYPE_ADMIN;
	_typemap["judge"] = USER_TYPE_JUDGE;
	_typemap["contestant"] = USER_TYPE_CONTESTANT;
	_typemap["marker"] = USER_TYPE_MARKER;
	_typemap["observer"] = USER_TYPE_OBSERVER;
	_typemap["proctor"] = USER_TYPE_PROCTOR;
}

auto_ptr<MessageBlock> ActAddUser::int_process(ClientConnection *cc, const MessageBlock *mb) {
	UserSupportModule *usm = getUserSupportModule();
	uint32_t user_id = cc->getUserId();
	string new_username = (*mb)["username"];
	string new_friendlyname = (*mb)["friendlyname"];
	string new_passwd = (*mb)["passwd"];
	uint32_t new_type = _typemap[(*mb)["type"]];
	uint32_t new_group = atol((*mb)["group"].c_str());

	if (!usm)
		return MessageBlock::error("UserSupportModule not available.");

	if(!new_type)
		return MessageBlock::error("Invalid user type " + (*mb)["type"]);

	if(new_username == "")
		return MessageBlock::error("Cannot have a blank username");

	if(new_passwd == "")
		return MessageBlock::error("Cannot set a blank password");

	if(usm->groupname(new_group) == "")
		return MessageBlock::error("Invalid group ID");

	if ((new_passwd = hashpw(new_username, new_passwd)) == "")
		return MessageBlock::error("Password hashing error");

	if (new_friendlyname == "")
		new_friendlyname = new_username;

	uint32_t tmp_user_id = usm->user_id(new_username);

	if(tmp_user_id == ~0U)
		return MessageBlock::error("Unexpected error while trying to query availability of username '" + new_username + "'");
	else if (tmp_user_id != 0)
		return MessageBlock::error("Username '" + new_username + "' is already in use");

	uint32_t new_id = usm->nextId();
	if(new_id == ~0U)
		return MessageBlock::error("Unable to determine next usable user_id");

	return triggerMessage(cc, new Message_CreateUser(new_username, new_friendlyname,
				new_passwd, new_id, new_type, new_group, user_id));
}

static ActAddUser _act_adduser;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("adduser", PERMISSION_USER_ADMIN, &_act_adduser);
}
