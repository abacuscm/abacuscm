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

using namespace std;

class ActPasswd : public ClientAction {
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

class ActIdPasswd : public ClientAction { /* Change other user's password */
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

class ActGetUsers : public ClientAction {
protected:
        virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

class PasswdMessage : public Message {
private:
	uint32_t _user_id;
	string _newpass;
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);
public:
	PasswdMessage();
	PasswdMessage(uint32_t user_id, const string& newpass);

	virtual bool process() const;

	virtual uint16_t message_type_id() const { return TYPE_ID_UPDATEPASS; }
};

bool ActPasswd::int_process(ClientConnection *cc, MessageBlock *mb) {
	UserSupportModule *usm = getUserSupportModule();
	uint32_t user_id = cc->getProperty("user_id");
	string newpass = (*mb)["newpass"];

	if (newpass == "")
		return cc->sendError("Cannot have a blank password!");

	if ((newpass = usm->hashpw(user_id, newpass)) == "")
		return cc->sendError("Error hashing password.");

	return triggerMessage(cc, new PasswdMessage(user_id, newpass));
}

bool ActIdPasswd::int_process(ClientConnection *cc, MessageBlock *mb) {
	UserSupportModule *usm = getUserSupportModule();
	char *errptr;
	uint32_t user_id = strtol((*mb)["user_id"].c_str(), &errptr, 0);
	if(*errptr || (*mb)["user_id"] == "")
		return cc->sendError("user_id isn't a valid integer");

	string newpass = (*mb)["newpass"];
	if (newpass == "")
		return cc->sendError("Cannot have a blank password!");

	if ((newpass = usm->hashpw(user_id, newpass)) == "")
		return cc->sendError("Invalid user id or hashing error.");

	return triggerMessage(cc, new PasswdMessage(user_id, newpass));
}

bool ActGetUsers::int_process(ClientConnection *cc, MessageBlock *) {
	UserSupportModule *usm = getUserSupportModule();
	if(!usm)
		return cc->sendError("Misconfigured server, no UserSupportModule!");

	UserSupportModule::UserList lst = usm->list();

	MessageBlock res("ok");

	UserSupportModule::UserList::iterator s;
	int c = 0;
	for (s = lst.begin(); s != lst.end(); ++s, ++c) {
		std::ostringstream tmp;
		tmp << c;
		std::string cntr = tmp.str();

		tmp.str(""); tmp << s->user_id;
		res["id" + cntr] = tmp.str();
		res["username" + cntr] = s->username;
	}

	return cc->sendMessageBlock(&res);
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

bool PasswdMessage::process() const {
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

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "passwd", &_act_passwd);
	ClientAction::registerAction(USER_TYPE_JUDGE, "passwd", &_act_passwd);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "passwd", &_act_passwd);
	ClientAction::registerAction(USER_TYPE_MARKER, "passwd", &_act_passwd);
	ClientAction::registerAction(USER_TYPE_ADMIN, "id_passwd", &_act_id_passwd);
	ClientAction::registerAction(USER_TYPE_ADMIN, "getusers", &_act_get_users);
	Message::registerMessageFunctor(TYPE_ID_UPDATEPASS, create_passwd_msg);
}
