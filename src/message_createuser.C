/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "message_createuser.h"
#include "message_type_ids.h"
#include "logger.h"
#include "messageblock.h"
#include "clienteventregistry.h"
#include "usersupportmodule.h"

Message_CreateUser::Message_CreateUser() {
}

Message_CreateUser::Message_CreateUser(const std::string& username, const std::string& friendlyname, const std::string& pass, uint32_t id, uint32_t type, uint32_t requestor_id) {
	_username = username;
	_friendlyname = friendlyname;
	_password = pass;
	_user_id = id;
	_type = type;
	_requestor_id = requestor_id;
}

Message_CreateUser::~Message_CreateUser() {
}

bool Message_CreateUser::int_process() const {
	UserSupportModule *usm = getUserSupportModule();

	bool added = usm && usm->addUser(_user_id, _username, _friendlyname, _password, _type);

	if(added)
		log(LOG_NOTICE, "Added user '%s'", _username.c_str());
	else
		log(LOG_ERR, "Failed adding user '%s'", _username.c_str());

	if(_requestor_id) {
		MessageBlock mb("msg");
		mb["title"] = "User addition";
		if(added)
			mb["text"] = "User '" + _username + "' was successfully added.";
		else
			mb["text"] = "Addition of user '" + _username + "' failed!";
		ClientEventRegistry::getInstance().sendMessage(_requestor_id, &mb);
	}

	return added;
}

uint16_t Message_CreateUser::message_type_id() const {
	return TYPE_ID_CREATEUSER;
}

uint32_t Message_CreateUser::storageRequired() {
	return _username.length() + _friendlyname.length() + _password.length() + 3 + 3 * sizeof(uint32_t);
}

uint32_t Message_CreateUser::store(uint8_t *buffer, uint32_t size) {
	if(size < storageRequired())
		return ~0U;
	char* pos = (char*)buffer;
	*(uint32_t*)pos = _user_id; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _type; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _requestor_id; pos += sizeof(uint32_t);
	strcpy(pos, _username.c_str()); pos += _username.length() + 1;
	strcpy(pos, _friendlyname.c_str()); pos += _friendlyname.length() + 1;
	strcpy(pos, _password.c_str()); pos += _password.length() + 1;

	return (uint8_t*)pos - buffer;
}

uint32_t Message_CreateUser::load(const uint8_t *buffer, uint32_t size) {
	uint32_t numzeros = 0;
	const char *pos = (const char*)buffer;
	for(uint32_t offset = 3 * sizeof(uint32_t); offset < size; offset++)
		if(!buffer[offset])
			numzeros++;
	if(numzeros < 3)
		return ~0U;

	_user_id = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_type = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_requestor_id = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_username = std::string(pos); pos += _username.length() + 1;
	_friendlyname = std::string(pos); pos += _friendlyname.length() + 1;
	_password = std::string(pos); pos += _password.length() + 1;

	return (uint8_t*)pos - buffer;
}
