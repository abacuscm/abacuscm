#include "clientaction.h"
#include "clientconnection.h"
#include "logger.h"
#include "message.h"
#include "messageblock.h"
#include "message_type_ids.h"
#include "dbcon.h"
#include "server.h"
#include "eventregister.h"
#include "markers.h"

#include <sstream>
#include <string>
#include <time.h>

class ActGetClarifications : public ClientAction {
protected:
	bool int_process(ClientConnection *cc, MessageBlock *mb);
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

	virtual bool process() const;

	virtual uint16_t message_type_id() const;
};

class ActClarificationRequest : public ClientAction {
protected:
	bool int_process(ClientConnection *cc, MessageBlock *sb);
};

class ActClarification : public ClientAction {
protected:
	bool int_process(ClientConnection *cc, MessageBlock *sb);
};

bool ActGetClarifications::int_process(ClientConnection *cc, MessageBlock *) {
	DbCon *db = DbCon::getInstance();
	if (!db)
		return cc->sendError("Error connecting to database");

	uint32_t uid = cc->getProperty("user_id");
	uint32_t utype = cc->getProperty("user_type");

	ClarificationList lst;

	if(utype == USER_TYPE_CONTESTANT)
		lst = db->getClarifications(uid);
	else
		lst = db->getClarifications();

	MessageBlock mb("ok");

	ClarificationList::iterator s;
	int c = 0;
	for (s = lst.begin(); s != lst.end(); ++s, ++c) {
		std::ostringstream tmp;
		tmp << c;
		std::string cntr = tmp.str();

		AttributeList::iterator a;
		for(a = s->begin(); a != s->end(); ++a)
			mb[a->first + cntr] = a->second;
	}

	return cc->sendMessageBlock(&mb);
}

class ActGetClarificationRequests : public ClientAction {
protected:
	bool int_process(ClientConnection *cc, MessageBlock *mb);
};

bool ActGetClarificationRequests::int_process(ClientConnection *cc, MessageBlock *) {
	DbCon *db = DbCon::getInstance();
	if (!db)
		return cc->sendError("Error connecting to database");

	uint32_t uid = cc->getProperty("user_id");
	uint32_t utype = cc->getProperty("user_type");

	ClarificationRequestList lst;

	if(utype == USER_TYPE_CONTESTANT)
		lst = db->getClarificationRequests(uid);
	else
		lst = db->getClarificationRequests();

	MessageBlock mb("ok");

	ClarificationRequestList::iterator s;
	int c = 0;
	for (s = lst.begin(); s != lst.end(); ++s, ++c) {
		std::ostringstream tmp;
		tmp << c;
		std::string cntr = tmp.str();

		AttributeList::iterator a;
		for(a = s->begin(); a != s->end(); ++a)
			mb[a->first + cntr] = a->second;
	}

	return cc->sendMessageBlock(&mb);
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

bool ClarificationMessage::process() const {
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
	}
	else
	{
		result = db->putClarification(_clarification_request_id,
					      _clarification_id,
					      _user_id,
					      _time,
					      _server_id,
					      _prob_id, /* Actually pub */
					      _text);
	}
	db->release();
	return result;
}

bool ActClarificationRequest::int_process(ClientConnection *cc, MessageBlock *mb) {
	uint32_t user_id = cc->getProperty("user_id");
	uint32_t prob_id = 0;
	std::string prob_id_str = (*mb)["prob_id"];
	std::string question = (*mb)["question"];

	if (prob_id_str != "0" && prob_id_str != "")
	{
		char *errptr;
		prob_id = strtol((*mb)["prob_id"].c_str(), &errptr, 0);
		if(*errptr || (*mb)["prob_id"] == "")
			return cc->sendError("prob_id isn't a valid integer");

		DbCon *db = DbCon::getInstance();
		if(!db)
			return cc->sendError("Unable to connect to database");
		ProblemList probs = db->getProblems();
		db->release();

		ProblemList::iterator p;
		for(p = probs.begin(); p != probs.end(); ++p)
			if(*p == prob_id)
				break;
		if(p == probs.end())
			return cc->sendError("Invalid prob_id - no such id");
	}

	uint32_t cr_id = Server::nextClarificationRequestId();
	if(cr_id == ~0U)
		return cc->sendError("Internal server error. Error obtaining new clarification request id");

	ClarificationMessage *msg = new ClarificationMessage(cr_id, prob_id, user_id, question);
	log(LOG_INFO, "User %u submitted clarification request for problem %u", user_id, prob_id);
	if (!triggerMessage(cc, msg)) return false;

	MessageBlock notify("updateclarificationrequests");
	EventRegister::getInstance().broadcastEvent(&notify);
	return true;
}

bool ActClarification::int_process(ClientConnection *cc, MessageBlock *mb) {
	uint32_t user_id = cc->getProperty("user_id");
	uint32_t req_user_id = 0;
	std::string answer = (*mb)["answer"];

	char *errptr;
	uint32_t cr_id = strtol((*mb)["clarification_request_id"].c_str(), &errptr, 0);
	if(*errptr || (*mb)["clarification_request_id"] == "")
		return cc->sendError("clarification_request_id isn't a valid integer");
	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Unable to connect to database");
	ClarificationRequestList crs = db->getClarificationRequests();
	db->release();
	ClarificationRequestList::iterator c;
	for(c = crs.begin(); c != crs.end(); ++c)
		if((*c)["id"] == (*mb)["clarification_request_id"])
		{
			req_user_id = atol((*c)["user_id"].c_str());
			break;
		}
	if(c == crs.end())
		return cc->sendError("Invalid clarification_request_id - no such id");

	uint32_t pub = (*mb)["public"] == "1";
	uint32_t c_id = Server::nextClarificationId();
	if(c_id == ~0U)
		return cc->sendError("Internal server error. Error obtaining new clarification id");

	ClarificationMessage *msg = new ClarificationMessage(cr_id, c_id, user_id, pub, answer);
	log(LOG_INFO, "User %u submitted clarification for request %u", user_id, cr_id);
	if (!triggerMessage(cc, msg)) return false;

	MessageBlock notify("updateclarifications");
	EventRegister::getInstance().broadcastEvent(&notify);

	MessageBlock alert("msg");
	alert["title"] = "New clarification";
	alert["text"] = answer;
	if (pub)
		EventRegister::getInstance().broadcastEvent(&alert);
	else
		EventRegister::getInstance().sendMessage(req_user_id, &alert);
	return true;
}

////////////////////////////////////////////////////////////////////
static Message* create_clarification_msg() {
	return new ClarificationMessage();
}

static ActGetClarifications _act_getclarifications;
static ActGetClarificationRequests _act_getclarificationrequests;
static ActClarificationRequest _act_clarificationrequest;
static ActClarification _act_clarification;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "getclarifications", &_act_getclarifications);
	ClientAction::registerAction(USER_TYPE_JUDGE, "getclarifications", &_act_getclarifications);
	ClientAction::registerAction(USER_TYPE_ADMIN, "getclarifications", &_act_getclarifications);

	ClientAction::registerAction(USER_TYPE_CONTESTANT, "getclarificationrequests", &_act_getclarificationrequests);
	ClientAction::registerAction(USER_TYPE_JUDGE, "getclarificationrequests", &_act_getclarificationrequests);
	ClientAction::registerAction(USER_TYPE_ADMIN, "getclarificationrequests", &_act_getclarificationrequests);

	ClientAction::registerAction(USER_TYPE_CONTESTANT, "clarificationrequest", &_act_clarificationrequest);
	ClientAction::registerAction(USER_TYPE_JUDGE, "clarificationrequest", &_act_clarificationrequest);
	ClientAction::registerAction(USER_TYPE_ADMIN, "clarificationrequest", &_act_clarificationrequest);

	ClientAction::registerAction(USER_TYPE_JUDGE, "clarification", &_act_clarification);
	ClientAction::registerAction(USER_TYPE_ADMIN, "clarification", &_act_clarification);

	Message::registerMessageFunctor(TYPE_ID_CLARIFICATION, create_clarification_msg);

	EventRegister::getInstance().registerEvent("updateclarifications");
	EventRegister::getInstance().registerEvent("updateclarificationrequests");
}
