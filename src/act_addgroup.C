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
#include "misc.h"

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

class ActGetGroups : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

auto_ptr<MessageBlock> ActGetGroups::int_process(ClientConnection *, const MessageBlock *) {
	UserSupportModule *usm = getUserSupportModule();
	if(!usm)
		return MessageBlock::error("Misconfigured server, no UserSupportModule!");

	UserSupportModule::GroupList groups = usm->groupList();
	auto_ptr<MessageBlock> res(MessageBlock::ok());

	UserSupportModule::GroupList::iterator s;
	int c = 0;
	for (s = groups.begin(); s != groups.end(); ++s, ++c) {
		std::string cntr = to_string(c);
		(*res)["id" + cntr] = to_string(s->group_id);
		(*res)["groupname" + cntr] = s->groupname;
	}

	return res;
}

static ActAddGroup _act_addgroup;
static ActGetGroups _act_getgroups;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("addgroup", PERMISSION_USER_ADMIN, &_act_addgroup);
	ClientAction::registerAction("getgroups", PermissionTest(PERMISSION_USER_ADMIN) || PERMISSION_START_STOP, &_act_getgroups);
}
