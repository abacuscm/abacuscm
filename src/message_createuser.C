#include "message_createuser.h"
#include "message_type_ids.h"
#include "logger.h"
#include "dbcon.h"

Message_CreateUser::Message_CreateUser(const std::string& name, const std::string& pass, uint32_t id, uint32_t type) {
	_name = name;
	_password = pass;
	_user_id = id;
	_type = type;
}

Message_CreateUser::~Message_CreateUser() {
}

bool Message_CreateUser::process() const {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return false;
	bool added = db->addUser(_name, _password, _user_id, _type);
	db->release();

	if(added)
		log(LOG_NOTICE, "Added user '%s'", _name.c_str());
	else
		log(LOG_ERR, "Failed adding user '%s'", _name.c_str());

	return added;
}

uint16_t Message_CreateUser::message_type_id() const {
	return TYPE_ID_CREATEUSER;
}

uint32_t Message_CreateUser::storageRequired() {
	return _name.length() + _password.length() + 2 + 2 * sizeof(uint32_t);
}

uint32_t Message_CreateUser::store(uint8_t *buffer, uint32_t size) {
	if(size < storageRequired())
		return ~0U;
	char* pos = (char*)buffer;
	*(uint32_t*)pos = _user_id; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _type; pos += sizeof(uint32_t);
	strcpy(pos, _name.c_str()); pos += _name.length() + 1;
	strcpy(pos, _password.c_str()); pos += _password.length() + 1;
	
	return (uint8_t*)pos - buffer;
}

uint32_t Message_CreateUser::load(const uint8_t *buffer, uint32_t size) {
	uint32_t numzeros = 0;
	const char *pos = (const char*)buffer;
	for(uint32_t offset = 2 * sizeof(uint32_t); offset < size; offset++)
		if(!buffer[offset])
			numzeros++;
	if(numzeros < 2)
		return ~0U;

	_user_id = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_type = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_name = std::string(pos); pos += _name.length() + 1;
	_password = std::string(pos); pos += _password.length() + 1;
	
	return (uint8_t*)pos - buffer;
}