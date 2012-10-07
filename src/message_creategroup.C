/**
 * Copyright (c) 2012 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "message_creategroup.h"
#include "message_type_ids.h"
#include "logger.h"
#include "usersupportmodule.h"
#include "timersupportmodule.h"
#include <cstring>

using namespace std;

Message_CreateGroup::Message_CreateGroup() {
}

Message_CreateGroup::Message_CreateGroup(const std::string& groupname, uint32_t id) {
	_groupname = groupname;
	_group_id = id;
}

bool Message_CreateGroup::int_process() const {
	UserSupportModule *usm = getUserSupportModule();
	TimerSupportModule *timer = getTimerSupportModule();

	bool added = usm && timer && usm->addGroup(_group_id, _groupname);

	if (added)
		log(LOG_NOTICE, "Added group '%s'", _groupname.c_str());
	else
		log(LOG_ERR, "Failed adding group '%s'", _groupname.c_str());

	int old_state, new_state;
	timer->updateGroupState(_group_id, ::time(NULL), old_state, new_state);

	return added;
}

uint16_t Message_CreateGroup::message_type_id() const {
	return TYPE_ID_CREATEGROUP;
}

uint32_t Message_CreateGroup::storageRequired() {
	return _groupname.length() + 1 + sizeof(uint32_t);
}

uint32_t Message_CreateGroup::store(uint8_t *buffer, uint32_t size) {
	if (size < storageRequired())
		return ~0U;
	char* pos = (char*) buffer;
	*(uint32_t *) pos = _group_id; pos += sizeof(uint32_t);
	strcpy(pos, _groupname.c_str()); pos += _groupname.length() + 1;

	return (uint8_t*)pos - buffer;
}

uint32_t Message_CreateGroup::load(const uint8_t *buffer, uint32_t size) {
	uint32_t numzeros = 0;
	const char *pos = (const char *) buffer;
	for (uint32_t offset = sizeof(uint32_t); offset < size; offset++)
		if (!buffer[offset])
			numzeros++;
	if (numzeros < 1)
		return ~0U;

	_group_id = *(uint32_t *) pos; pos += sizeof(uint32_t);
	_groupname = std::string(pos); pos += _groupname.length() + 1;

	return (uint8_t *) pos - buffer;
}
