/**
 * Copyright (c) 2012 Kroon Infomation Systems,
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
#include "usersupportmodule.h"
#include "message_creategroup.h"

using namespace std;

class ActAddGroup : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

auto_ptr<MessageBlock> ActAddGroup::int_process(ClientConnection *cc, const MessageBlock *mb) {
	UserSupportModule *usm = getUserSupportModule();
	string groupname = (*mb)["groupname"];

	if (!usm)
		return MessageBlock::error("UserSupportModule not available.");
	if (groupname == "")
		return MessageBlock::error("Cannot have a blank group name");

	uint32_t tmp_group_id = usm->group_id(groupname);

	if (tmp_group_id == ~0U)
		return MessageBlock::error("Unexpected error while trying to query availability of group name '" + groupname + "'");
	else if (tmp_group_id != 0)
		return MessageBlock::error("Group '" + groupname + "' already exists");

	uint32_t new_id = usm->nextGroupId();
	if (new_id == ~0U)
		return MessageBlock::error("Unable to determine next usable group id");

	return triggerMessage(cc, new Message_CreateGroup(groupname, new_id));
}

static ActAddGroup _act_addgroup;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("addgroup", PERMISSION_USER_ADMIN, &_act_addgroup);
}
