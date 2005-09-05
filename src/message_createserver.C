#include "message_createserver.h"
#include "logger.h"
#include "dbcon.h"
#include "server.h"
#include "peermessenger.h"

#include "message_type_ids.h"

using namespace std;

Message_CreateServer::Message_CreateServer() {
}

Message_CreateServer::Message_CreateServer(const string& name, uint32_t id) {
	_name = name;
	_server_id = id;
}

Message_CreateServer::~Message_CreateServer() {
}

uint32_t Message_CreateServer::storageRequired() {
	uint32_t sreq = sizeof(_server_id) + _name.length() + 1;
	AttribMap::const_iterator i;
	for(i = _attribs.begin(); i != _attribs.end(); ++i)
		sreq += i->first.length() + i->second.length() + 2;
	return sreq;
}

uint32_t Message_CreateServer::store(uint8_t *buffer, uint32_t size) {
	if(size < storageRequired())
		return ~0U;
	char* pos = (char*)buffer;
	*(uint32_t*)pos = _server_id; pos += sizeof(uint32_t);
	strcpy(pos, _name.c_str()); pos += _name.length() + 1;
	AttribMap::const_iterator i;
	for(i = _attribs.begin(); i != _attribs.end(); ++i) {
		strcpy(pos, i->first.c_str()); pos += i->first.length() + 1;
		strcpy(pos, i->second.c_str()); pos += i->second.length() + 1;
	}
	uint32_t used = (uint8_t*)pos - buffer;
	if(used > size)
		log(LOG_CRIT, "Buffer overrun in Message_CreateServer::store (server_id=%d, message_id=%d) - expect segfault!", server_id(), message_id());

	return used;
}

uint32_t Message_CreateServer::load(const uint8_t *buffer, uint32_t size) {
	uint32_t numzeros = 0;
	const char *pos = (const char*)buffer;
	// check for a valid packet.
	for(uint32_t offset = sizeof(uint32_t); offset < size; offset++)
		if(!buffer[offset])
			numzeros++;
	if(!numzeros)
		return ~0U;

	_server_id = *(uint32_t*)pos;
	pos += sizeof(uint32_t);
	
	_name = string(pos);
	pos += _name.length() + 1;
	numzeros--;
	
	while(numzeros > 1) {
		string attrib_name = string(pos);
		pos += attrib_name.length() + 1;
		string attrib_value = string(pos);
		pos += attrib_value.length() + 1;

		_attribs[attrib_name] = attrib_value;

		numzeros -= 2;
	}
	
	return (uint8_t*)pos - buffer;
}

void Message_CreateServer::addAttribute(const string& name, const string& value) {
	_attribs[name] = value;
}

bool Message_CreateServer::process() const {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return false;
	if(db->addServer(_name, _server_id)) {
		log(LOG_NOTICE, "Added server '%s'", _name.c_str());

		AttribMap::const_iterator i;
		for(i = _attribs.begin(); i != _attribs.end(); ++i)
			if(!db->setServerAttribute(_server_id, i->first, i->second))
				log(LOG_WARNING, "Unable to set attribute '%s' = '%s' for '%s'.", i->first.c_str(), i->second.c_str(), _name.c_str());

		db->release();

		Server::putAck(_server_id, 0, 0);

		return true;
	} else {
		db->release();
		return false;
	}
}

uint16_t Message_CreateServer::message_type_id() const {
	return TYPE_ID_CREATESERVER;
}
