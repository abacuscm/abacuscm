#include "message.h"
#include "logger.h"
#include "server.h"
#include "dbcon.h"

#include <time.h>

Message::Message() {
	_server_id = 0;
	_message_id = 0;
	_time = 0;
	_blob_size = 0;
	_blob = NULL;
	_signature = NULL;
}

Message::~Message() {
}

bool Message::makeMessage() {
	if(_server_id || _message_id || _time) {
		log(LOG_ERR, "Weird, trying to make a message from an already perfectly good message!");
		return false;
	}

	uint32_t size = storageRequired();
	
	// for the message_type_id we need to rebuild again...
	if(size == ~0U) {
		log(LOG_ERR, "Unable to determine size requirements for packing Message into a blob or overflow error!");
		return false;
	}

	try {
		uint8_t *buffer = new uint8_t[size];
		uint32_t used = store(buffer, size);
		if(used > size) {
			// this should _never_ happen!
			log(LOG_ERR, "Buffer overrun in Message::makeBlob() - expect a segfault");
			delete []buffer;
			return false;
		}

		_server_id = Server::getId();
		_time = (uint32_t)::time(NULL);
	
		_blob = buffer;
		_blob_size = size;

		DbCon *db = DbCon::getInstance();
		db->putLocalMessage(this);
		db->release();

		return true;
	} catch(...) {
		log(LOG_ERR, "Failed to pack message, exception caught");
		return false;
		// I only ever expect a memory exception...
	}
}

/*
uint32_t Message::storageRequired() {
	return 0;
}

uint32_t Message::store(uint8_t * buffer, uint32_t size) {
	return 0;
}

uint32_t Message::load(const uint8_t * buffer, uint32_t size) {
	return 0;
}
*/

void Message::setMessageId(uint32_t id) {
	if(!_server_id && !_time)
		return;
	_message_id = id;
	_signature = new uint8_t[MESSAGE_SIGNATURE_SIZE];
	memset(_signature, 0, MESSAGE_SIGNATURE_SIZE);
}

bool Message::getBlob(const uint8_t ** buffer, uint32_t *size) const {
	*buffer = _blob;
	*size = _blob_size;
	return _blob != 0;
}
