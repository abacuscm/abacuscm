#include "message_createserver.h"
#include "logger.h"

#include "message_type_ids.h"

using namespace std;

Message_CreateServer::Message_CreateServer(const string& name, uint32_t id) {
	_name = name;
	_server_id = id;
}

Message_CreateServer::~Message_CreateServer() {
}

uint32_t Message_CreateServer::storageRequired() {
	return sizeof(_server_id) + _name.length() + 1;
}

uint32_t Message_CreateServer::store(uint8_t *buffer, uint32_t size) {
	if(size < storageRequired())
		return ~0U;
	*(uint32_t*)buffer = _server_id;
	strcpy((char*)buffer + sizeof(_server_id), _name.c_str());
	return storageRequired();
}

uint32_t Message_CreateServer::load(const uint8_t *buffer, uint32_t size) {
	_server_id = *(uint32_t*)buffer;
	buffer += sizeof(uint32_t);
	_name = (char*)buffer;
	return storageRequired();
}

void Message_CreateServer::addAttribute(const string& name, const string& value) {
	NOT_IMPLEMENTED();
}

void Message_CreateServer::process() const {
	NOT_IMPLEMENTED();
}

uint16_t Message_CreateServer::message_type_id() const {
	return TYPE_ID_CREATESERVER;
}
