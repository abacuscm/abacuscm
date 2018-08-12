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
#include "logger.h"
#include "message.h"
#include "messageblock.h"
#include "message_type_ids.h"
#include "dbcon.h"
#include "server.h"
#include "clienteventregistry.h"
#include "markers.h"
#include "misc.h"

#include <string>
#include <memory>
#include <cstring>
#include <time.h>

using namespace std;

class ActGetClarifications : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

/* Holds both requests and replies */
class ClarificationMessage : public Message {
private:
	uint32_t _request;
	uint32_t _clarification_request_id;
	uint32_t _clarification_id;
	uint32_t _prob_id;  /* Holds "public" for replies" */
	uint32_t _user_id;
	uint32_t _time;
	uint32_t _server_id;
	std::string _text;
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t* buffer, uint32_t size);
	virtual uint32_t load(const uint8_t* buffer, uint32_t size);

	virtual bool int_process() const;
public:
	ClarificationMessage();
	ClarificationMessage(uint32_t clarification_request_id,
				 uint32_t prob_id,
				 uint32_t user_id,
				 const std::string& question);
	ClarificationMessage(uint32_t clarification_request_id,
				 uint32_t clarification_id,
				 uint32_t user_id,
				 bool pub,
				 const std::string& answer);
	virtual ~ClarificationMessage();

	virtual uint16_t message_type_id() const;
};

class ActClarificationRequest : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *sb);
};

class ActClarification : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *sb);
};

auto_ptr<MessageBlock> ActGetClarifications::int_process(ClientConnection *cc, const MessageBlock *) {
	DbCon *db = DbCon::getInstance();
	if (!db)
		return MessageBlock::error("Error connecting to database");

	uint32_t uid = cc->getUserId();

	ClarificationList lst;

	if(cc->permissions()[PERMISSION_SEE_ALL_CLARIFICATIONS])
		lst = db->getClarifications();
	else
		lst = db->getClarifications(uid);
	db->release();db=NULL;

	auto_ptr<MessageBlock> mb(MessageBlock::ok());

	ClarificationList::iterator s;
	int c = 0;
	for (s = lst.begin(); s != lst.end(); ++s, ++c) {
		std::string cntr = to_string(c);

		AttributeList::iterator a;
		for(a = s->begin(); a != s->end(); ++a)
			(*mb)[a->first + cntr] = a->second;
	}

	return mb;
}

class ActGetClarificationRequests : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

auto_ptr<MessageBlock> ActGetClarificationRequests::int_process(ClientConnection *cc, const MessageBlock *) {
	DbCon *db = DbCon::getInstance();
	if (!db)
		return MessageBlock::error("Error connecting to database");

	uint32_t uid = cc->getUserId();

	ClarificationRequestList lst;

	if(!cc->permissions()[PERMISSION_SEE_ALL_CLARIFICATION_REQUESTS])
		lst = db->getClarificationRequests(uid);
	else
		lst = db->getClarificationRequests();
	db->release();db=NULL;

	auto_ptr<MessageBlock> mb(MessageBlock::ok());

	ClarificationRequestList::iterator s;
	int c = 0;
	for (s = lst.begin(); s != lst.end(); ++s, ++c) {
		std::string cntr = to_string(c);

		AttributeList::iterator a;
		for(a = s->begin(); a != s->end(); ++a)
			(*mb)[a->first + cntr] = a->second;
	}

	return mb;
}

ClarificationMessage::ClarificationMessage() {
	_request = 0;
	_clarification_request_id = 0;
	_clarification_id = 0;
	_prob_id = 0;
	_user_id = 0;
	_time = 0;
	_server_id = 0;
	_text = "";
}

ClarificationMessage::ClarificationMessage(uint32_t clarification_request_id,
										   uint32_t prob_id,
										   uint32_t user_id,
										   const std::string& question) {
	_request = 1;
	_clarification_request_id = clarification_request_id;
	_clarification_id = 0;
	_prob_id = prob_id;
	_user_id = user_id;
	_time = ::time(NULL);
	_server_id = Server::getId();
	_text = question;
}

ClarificationMessage::ClarificationMessage(uint32_t clarification_request_id,
					   uint32_t clarification_id,
					   uint32_t user_id,
					   bool pub,
					   const std::string& answer) {
	_request = 0;
	_clarification_request_id = clarification_request_id;
	_clarification_id = clarification_id;
	_user_id = user_id;
	_time = ::time(NULL);
	_server_id = Server::getId();
	_prob_id = pub ? 1 : 0;
	_text = answer;
}

ClarificationMessage::~ClarificationMessage() {
}

uint16_t ClarificationMessage::message_type_id() const {
	return TYPE_ID_CLARIFICATION;
}

uint32_t ClarificationMessage::storageRequired() {
	return 7 * sizeof(uint32_t) + _text.length() + 1;
}

uint32_t ClarificationMessage::store(uint8_t *buffer, uint32_t size) {
	uint8_t *pos = buffer;
	*(uint32_t*)pos = _request; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _clarification_request_id; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _clarification_id; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _user_id; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _prob_id; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _time; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _server_id; pos += sizeof(uint32_t);
	strcpy((char*)pos, _text.c_str()); pos += _text.length() + 1;

	if((unsigned)(pos - buffer) > size)
		log(LOG_WARNING, "Buffer overflow detected - expect segfaults (Error is in class ClarificationMessage)");

	return pos - buffer;
}

uint32_t ClarificationMessage::load(const uint8_t *buffer, uint32_t size) {
	const uint8_t *pos = buffer;
	if (size <= 7 * sizeof(uint32_t)) {
		log(LOG_ERR, "Too small buffer to contain the correct number of uint32_t values in ClarificationRequestMessage::load()");
		return ~0U;
	}
	_request = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_clarification_request_id = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_clarification_id = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_user_id = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_prob_id = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_time = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_server_id = *(uint32_t*)pos; pos += sizeof(uint32_t);
	size -= 7 * sizeof(uint32_t);

	if(!checkStringTerms(pos, size)) {
		log(LOG_ERR, "Incomplete string in buffer decoding");
		return ~0U;
	}

	_text = (char*)pos;
	pos += _text.length() + 1;

	return pos - buffer;
}

bool ClarificationMessage::int_process() const {
	DbCon *db = DbCon::getInstance();
	if (!db)
		return false;

	bool result;
	if (_request)
	{
		result = db->putClarificationRequest(_clarification_request_id,
							 _user_id,
							 _prob_id,
							 _time,
							 _server_id,
							 _text);
		if (result) {
			/* FIXME: this is a nasty hack to get back the information about
			 * problem names etc.
			 */
			AttributeList req = db->getClarificationRequest(_clarification_request_id);
			if (req.empty())
				result = false;
			else {
				MessageBlock notify("updateclarificationrequests");
				AttributeList::iterator a;
				for(a = req.begin(); a != req.end(); ++a)
					notify[a->first] = a->second;
				ClientEventRegistry::getInstance().broadcastEvent(
					_user_id,
					PERMISSION_SEE_ALL_CLARIFICATION_REQUESTS,
					&notify);
			}
		}
	}
	else
	{
		int pub = _prob_id;   /* _prob_id field is overloaded for public */
		AttributeList req = db->getClarificationRequest(_clarification_request_id);
		result = db->putClarification(_clarification_request_id,
						  _clarification_id,
						  _user_id,
						  _time,
						  _server_id,
						  pub,
						  _text);
		if (result) {
			/* FIXME: this is a nasty hack to get back the information about
			 * problem names etc.
			 */
			AttributeList c = db->getClarification(_clarification_id);
			if (req.empty())
				result = false;
			else {
				MessageBlock notify("updateclarifications");
				AttributeList::iterator a;
				for(a = c.begin(); a != c.end(); ++a)
					notify[a->first] = a->second;
				if (pub)
					ClientEventRegistry::getInstance().broadcastEvent(
						0, PERMISSION_AUTH, &notify);
				else
					ClientEventRegistry::getInstance().broadcastEvent(
						from_string<uint32_t>(req["user_id"]),
						PERMISSION_SEE_ALL_CLARIFICATIONS,
						&notify);
			}
		}
	}
	db->release();db=NULL;
	return result;
}

auto_ptr<MessageBlock> ActClarificationRequest::int_process(ClientConnection *cc, const MessageBlock *mb) {
	uint32_t user_id = cc->getUserId();
	uint32_t prob_id = 0;
	std::string prob_id_str = (*mb)["prob_id"];
	std::string question = (*mb)["question"];

	if (prob_id_str != "0" && prob_id_str != "")
	{
		if (!from_string((*mb)["prob_id"], prob_id))
			return MessageBlock::error("prob_id isn't a valid integer");

		DbCon *db = DbCon::getInstance();
		if(!db)
			return MessageBlock::error("Unable to connect to database");
		ProblemList probs = db->getProblems();
		db->release();db=NULL;

		ProblemList::iterator p;
		for(p = probs.begin(); p != probs.end(); ++p)
			if(*p == prob_id)
				break;
		if(p == probs.end())
			return MessageBlock::error("Invalid prob_id - no such id");
	}

	uint32_t cr_id = Server::nextClarificationRequestId();
	if(cr_id == ~0U)
		return MessageBlock::error("Internal server error. Error obtaining new clarification request id");

	ClarificationMessage *msg = new ClarificationMessage(cr_id, prob_id, user_id, question);
	log(LOG_INFO, "User %u submitted clarification request for problem %u", user_id, prob_id);
	return triggerMessage(cc, msg);
}

auto_ptr<MessageBlock> ActClarification::int_process(ClientConnection *cc, const MessageBlock *mb) {
	uint32_t user_id = cc->getUserId();
	std::string answer = (*mb)["answer"];

	uint32_t cr_id;
	if (!from_string((*mb)["clarification_request_id"], cr_id))
		return MessageBlock::error("clarification_request_id isn't a valid integer");
	DbCon *db = DbCon::getInstance();
	if(!db)
		return MessageBlock::error("Unable to connect to database");
	AttributeList cr = db->getClarificationRequest(cr_id);
	db->release();db=NULL;

	if(cr.empty())
		return MessageBlock::error("Invalid clarification_request_id - no such id");

	uint32_t pub = (*mb)["public"] == "1";
	uint32_t c_id = Server::nextClarificationId();
	if(c_id == ~0U)
		return MessageBlock::error("Internal server error. Error obtaining new clarification id");

	ClarificationMessage *msg = new ClarificationMessage(cr_id, c_id, user_id, pub, answer);
	log(LOG_INFO, "User %u submitted clarification %u for request %u", user_id, c_id, cr_id);
	return triggerMessage(cc, msg);
}

////////////////////////////////////////////////////////////////////
static Message* create_clarification_msg() {
	return new ClarificationMessage();
}

static ActGetClarifications _act_getclarifications;
static ActGetClarificationRequests _act_getclarificationrequests;
static ActClarificationRequest _act_clarificationrequest;
static ActClarification _act_clarification;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("getclarifications", PERMISSION_AUTH, &_act_getclarifications);
	ClientAction::registerAction("getclarificationrequests", PERMISSION_AUTH, &_act_getclarificationrequests);
	ClientAction::registerAction("clarificationrequest", PERMISSION_CLARIFICATION_REQUEST, &_act_clarificationrequest);
	ClientAction::registerAction("clarification", PERMISSION_CLARIFICATION_REPLY, &_act_clarification);

	Message::registerMessageFunctor(TYPE_ID_CLARIFICATION, create_clarification_msg);

	ClientEventRegistry::getInstance().registerEvent("updateclarifications", PERMISSION_AUTH);
	ClientEventRegistry::getInstance().registerEvent("updateclarificationrequests", PERMISSION_AUTH);
}
