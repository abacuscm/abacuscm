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
#include "usersupportmodule.h"
#include "messageblock.h"
#include "logger.h"
#include "dbcon.h"
#include "message_type_ids.h"
#include "message.h"

#include <string>
#include <sstream>
#include <memory>

using namespace std;

class ActPasswd : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

class ActIdPasswd : public ClientAction { /* Change other user's password */
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

class ActGetUsers : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

class PasswdMessage : public Message {
private:
	uint32_t _user_id;
	string _newpass;
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);

	virtual bool int_process() const;
public:
	PasswdMessage();
	PasswdMessage(uint32_t user_id, const string& newpass);

	virtual uint16_t message_type_id() const { return TYPE_ID_UPDATEPASS; }
};

auto_ptr<MessageBlock> ActPasswd::int_process(ClientConnection *cc, const MessageBlock *mb) {
	UserSupportModule *usm = getUserSupportModule();
	uint32_t user_id = cc->getUserId();
	string newpass = (*mb)["newpass"];

	if (newpass == "")
		return MessageBlock::error("Cannot have a blank password!");

	if ((newpass = usm->hashpw(user_id, newpass)) == "")
		return MessageBlock::error("Error hashing password.");

	return triggerMessage(cc, new PasswdMessage(user_id, newpass));
}

auto_ptr<MessageBlock> ActIdPasswd::int_process(ClientConnection *cc, const MessageBlock *mb) {
	UserSupportModule *usm = getUserSupportModule();
	char *errptr;
	uint32_t user_id = strtol((*mb)["user_id"].c_str(), &errptr, 0);
	if(*errptr || (*mb)["user_id"] == "")
		return MessageBlock::error("user_id isn't a valid integer");

	string newpass = (*mb)["newpass"];
	if (newpass == "")
		return MessageBlock::error("Cannot have a blank password!");

	if ((newpass = usm->hashpw(user_id, newpass)) == "")
		return MessageBlock::error("Invalid user id or hashing error.");

	return triggerMessage(cc, new PasswdMessage(user_id, newpass));
}

auto_ptr<MessageBlock> ActGetUsers::int_process(ClientConnection *, const MessageBlock *) {
	static const std::string typenames[] =
	{
		"none",
		"admin",
		"judge",
		"contestant",
		"marker",
		"observer",
		"proctor"
	};
	UserSupportModule *usm = getUserSupportModule();
	if(!usm)
		return MessageBlock::error("Misconfigured server, no UserSupportModule!");

	UserSupportModule::UserList lst = usm->userList();

	auto_ptr<MessageBlock> res(MessageBlock::ok());

	UserSupportModule::UserList::iterator s;
	int c = 0;
	for (s = lst.begin(); s != lst.end(); ++s, ++c) {
		std::ostringstream tmp;
		tmp << c;
		std::string cntr = tmp.str();

		tmp.str(""); tmp << s->user_id;
		(*res)["id" + cntr] = tmp.str();
		(*res)["username" + cntr] = s->username;
		(*res)["friendlyname" + cntr] = usm->friendlyname(s->user_id);
		uint32_t type = usm->usertype(s->user_id);
		if (type < sizeof(typenames) / sizeof(typenames[0]))
			(*res)["type" + cntr] = typenames[type];
		else
			return MessageBlock::error("Invalid user type");
	}

	return res;
}

PasswdMessage::PasswdMessage() {
}

PasswdMessage::PasswdMessage(uint32_t user_id, const string& newpass) {
	_user_id = user_id;
	_newpass = newpass;
}

uint32_t PasswdMessage::storageRequired() {
	return sizeof(_user_id) + _newpass.length() + 1;
}

uint32_t PasswdMessage::store(uint8_t *buffer, uint32_t size) {
	if(size < storageRequired())
		return ~0U;

	char *pos = (char*)buffer;

	*(uint32_t*)pos = _user_id;
	pos += sizeof(uint32_t);

	strcpy(pos, _newpass.c_str());

	return storageRequired();
}

uint32_t PasswdMessage::load(const uint8_t *buffer, uint32_t size) {
	if(size < sizeof(uint32_t))
		return ~0U;
	if(strnlen((char*)buffer + 4, size - sizeof(uint32_t)) == size - sizeof(uint32_t)) {
		return ~0U;
	}

	_user_id = *(uint32_t*)buffer;
	_newpass = (char*)buffer + sizeof(uint32_t);

	return storageRequired();
}

bool PasswdMessage::int_process() const {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return false;

	stringstream query;
	query << "UPDATE User SET password='" << db->escape_string(_newpass) << "' WHERE user_id=" << _user_id;
	bool result = db->executeQuery(query.str());
	db->release();
	return result;
}

static ActPasswd _act_passwd;
static ActIdPasswd _act_id_passwd;
static ActGetUsers _act_get_users;

static Message* create_passwd_msg() {
	return new PasswdMessage();
}

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("passwd", PERMISSION_CHANGE_PASSWORD, &_act_passwd);
	ClientAction::registerAction("id_passwd", PERMISSION_USER_ADMIN, &_act_id_passwd);
	ClientAction::registerAction("getusers", PERMISSION_USER_ADMIN, &_act_get_users);
	Message::registerMessageFunctor(TYPE_ID_UPDATEPASS, create_passwd_msg);
}
