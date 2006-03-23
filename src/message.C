/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "message.h"
#include "logger.h"
#include "server.h"
#include "dbcon.h"
#include "peermessenger.h"

#include <time.h>

std::map<uint32_t, MessageFunctor> Message::_functors;

Message::Message() {
	_server_id = 0;
	_message_id = 0;
	_time = 0;
	_blob_size = 0;
	_blob = NULL;
	_data_size = 0;
	_data = NULL;
	_signature = NULL;
}

Message::~Message() {
	if(_data)
		delete []_data;
	if(_blob)
		free(_blob);
	if(_signature)
		delete []_signature;
}

bool Message::makeMessage() {
	if(_server_id || _message_id || _time) {
		log(LOG_ERR, "Weird, trying to make a message from an already perfectly good message!");
		return false;
	}

	uint32_t size = storageRequired();

	if(size == ~0U) {
		log(LOG_ERR, "Unable to determine size requirements for packing Message into a blob or overflow error!");
		return false;
	}

	DbCon *db = NULL;
	try {
		uint8_t *buffer = new uint8_t[size];
		uint32_t used = store(buffer, size);
		if(used > size) {
			// this should _never_ happen!
			log(LOG_ERR, "Buffer overrun in %s - expect a segfault", __PRETTY_FUNCTION__);
			delete []buffer;
			return false;
		}

		_server_id = Server::getId();
		_time = (uint32_t)::time(NULL);

		_data = buffer;
		_data_size = size;

		db = DbCon::getInstance();
		if(!db->putLocalMessage(this)) {
			db->release();db=NULL;
			return false;
		}

		makeBlob();

		std::vector<uint32_t> remote_servers = db->getRemoteServers();

		std::vector<uint32_t>::iterator i;
		PeerMessenger *messenger = PeerMessenger::getMessenger();
		for(i = remote_servers.begin(); i != remote_servers.end(); ++i) {
			if(messenger->sendMessage(*i, this))
				log(LOG_INFO, "Message (%d,%d) sent to %d",
						_server_id, _message_id, *i);
//				db->setSent(_server_id, _message_id, *i);
		}
		db->release();db=NULL;
		return true;
	} catch(...) {
		log(LOG_ERR, "Failed to pack message, exception caught");
		if (db)
			db->release(); db = NULL;
		return false;
		// I only ever expect a memory exception...
	}
}

void Message::setMessageId(uint32_t id) {
	if(!_server_id && !_time)
		return;
	_message_id = id;
	_signature = new uint8_t[MESSAGE_SIGNATURE_SIZE];
	memset(_signature, 0, MESSAGE_SIGNATURE_SIZE);

	makeBlob();
}

bool Message::getBlob(const uint8_t ** buffer, uint32_t *size) const {
	*buffer = (uint8_t*)_blob;
	*size = _blob_size;
	return _blob != 0;
}

bool Message::getData(const uint8_t ** buffer, uint32_t *size) const {
	*buffer = (uint8_t*)_data;
	*size = _data_size;
	return _data != 0;
}

bool Message::buildMessage(uint32_t server_id, uint32_t message_id,
		uint32_t time, uint8_t *signature, uint8_t *data,
		uint32_t data_len, struct st_blob *blob, uint32_t blob_size) {
	uint32_t loadres = load(data, data_len);
	if(loadres == ~0U)
		return false;

	_server_id = server_id;
	_message_id = message_id;
	_time = time;
	_signature = signature;
	_data = data;
	_data_size = data_len;
	if(blob) {
		_blob = blob;
		_blob_size = blob_size;
	} else
		makeBlob();

	return true;
}

void Message::makeBlob() {
	_blob_size = sizeof(st_blob) + _data_size - 1;
	_blob = (struct st_blob*)malloc(_blob_size);
	if(!_blob) {
		log(LOG_CRIT, "Out of memory! (probably going to segfault just now)");
		_blob_size = 0;
		return;
	}
	_blob->server_id = _server_id;
	_blob->message_id = _message_id;
	_blob->time = _time;
	_blob->message_type_id = message_type_id();
	memcpy(_blob->signature, _signature, MESSAGE_SIGNATURE_SIZE);
	memcpy(_blob->data, _data, _data_size);
}

Message* Message::buildMessage(uint32_t server_id, uint32_t message_id,
		uint32_t message_type_id, uint32_t time, uint8_t *signature,
		uint8_t *data, uint32_t data_len) {
	MessageFunctor func = _functors[message_type_id];
	if(!func) {
		log(LOG_CRIT, "Unable to resolve message_type_id = %u",
				message_type_id);
		return NULL;
	}

	Message *tmp = func();

	if(~0U == tmp->buildMessage(server_id, message_id, time, signature, data,
				data_len, NULL, 0)) {
		delete tmp;
		return NULL;
	}

	return tmp;
}

Message* Message::buildMessage(uint8_t* blob_p, uint32_t blob_len) {
	if(blob_len < sizeof(struct st_blob))
		return NULL;

	struct st_blob *blob = (struct st_blob*)blob_p;

	MessageFunctor func = _functors[blob->message_type_id];

	if(!func) {
		log(LOG_CRIT, "Unable to resolve message_type_id = %u",
				blob->message_type_id);
		return NULL;
	}

	Message *tmp = func();
	uint32_t data_size = blob_len - sizeof(struct st_blob) + 1;
	uint8_t *data = new uint8_t[data_size];
	memcpy(data, blob->data, data_size);
	uint8_t *signature = new uint8_t[MESSAGE_SIGNATURE_SIZE];
	memcpy(signature, blob->signature, MESSAGE_SIGNATURE_SIZE);

	if(!tmp->buildMessage(blob->server_id, blob->message_id, blob->time,
			signature, data, data_size, blob, blob_len)) {
		delete []data;
		delete []signature;
		delete []tmp;
		return NULL;
	}

	return tmp;
}

void Message::registerMessageFunctor(uint32_t message_type_id, MessageFunctor func) {
	if(_functors[message_type_id] != 0)
		log(LOG_WARNING, "Re-registration of functor for message_type_id=%u",
				message_type_id);
	_functors[message_type_id] = func;
	log(LOG_INFO, "Registered functor for message_type_id=%u", message_type_id);
}

bool Message::checkStringTerms(const uint8_t* buf, uint32_t sz, uint32_t nzeros) {
	while(sz && nzeros) {
		if(!*buf++)
			--nzeros;
		--sz;
	}
	return nzeros == 0;
}
