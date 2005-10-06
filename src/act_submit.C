#include "clientaction.h"
#include "clientconnection.h"
#include "logger.h"
#include "message.h"
#include "messageblock.h"
#include "message_type_ids.h"
#include "dbcon.h"
#include "server.h"
#include "eventregister.h"

#include <sstream>

class ActSubmit : public ClientAction {
protected:
	bool int_process(ClientConnection *cc, MessageBlock *mb);
};

class ActGetProblems : public ClientAction {
protected:
	bool int_process(ClientConnection *cc, MessageBlock *mb);
};

class ActGetSubmissions : public ClientAction {
protected:
	bool int_process(ClientConnection *cc, MessageBlock *mb);
};

class SubmissionMessage : public Message {
private:
	uint32_t _prob_id;
	uint32_t _user_id;
	uint32_t _time;
	uint32_t _server_id;
	uint32_t _content_size;
	char* _content;
	std::string _language;
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t* buffer, uint32_t size);
	virtual uint32_t load(const uint8_t* buffer, uint32_t size);
public:
	SubmissionMessage();
	SubmissionMessage(uint32_t prob_id, uint32_t user_id, const char* content, uint32_t content_size, std::string language);
	virtual ~SubmissionMessage();
	
	virtual bool process() const;

	virtual uint16_t message_type_id() const;
};

bool ActSubmit::int_process(ClientConnection *cc, MessageBlock *mb) {
	uint32_t user_id = cc->getProperty("user_id");
	char *errptr;
	uint32_t prob_id = strtol((*mb)["prob_id"].c_str(), &errptr, 0);
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

	std::string lang = (*mb)["lang"];

	// TODO:  Proper check - this will do for now:
	if(lang != "C" && lang != "C++" && lang != "Java")
		return cc->sendError("You have not specified the language");
	
	SubmissionMessage *msg = new SubmissionMessage(prob_id, user_id,
			mb->content(), mb->content_size(), lang);

	log(LOG_INFO, "User %u submitted solution for problem %u", user_id, prob_id);

	return triggerMessage(cc, msg);
}

bool ActGetProblems::int_process(ClientConnection *cc, MessageBlock *) {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Error connecting to database");

	ProblemList probs = db->getProblems();
	db->release();

	MessageBlock mb("ok");

	ProblemList::iterator p;
	int c = 0;
	for(p = probs.begin(); p != probs.end(); ++p, ++c) {
		std::ostringstream ostrstrm;
		ostrstrm << c;
		std::string cstr = ostrstrm.str();
		ostrstrm.str("");
		ostrstrm << *p;
		
		mb["id" + cstr] = ostrstrm.str();
		
		AttributeList lst = db->getProblemAttributes(*p);
		mb["code" + cstr] = lst["shortname"];
		mb["name" + cstr] = lst["longname"];
	}
	
	return cc->sendMessageBlock(&mb);
}

bool ActGetSubmissions::int_process(ClientConnection *cc, MessageBlock *) {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Error connecting to database");
	
	uint32_t uid = cc->getProperty("user_id");
	uint32_t utype = cc->getProperty("user_type");

	SubmissionList lst;
	
	if(utype == USER_TYPE_CONTESTANT)
		lst = db->getSubmissions(uid);
	else
		lst = db->getSubmissions();

	MessageBlock mb("ok");
	
	SubmissionList::iterator s;
	int c = 0;
	for(s = lst.begin(); s != lst.end(); ++s, ++c) {
		std::ostringstream tmp;
		tmp << c;
		std::string cntr = tmp.str();

		AttributeList::iterator a;
		for(a = s->begin(); a != s->end(); ++a)
			mb[a->first + cntr] = a->second;
	}
	
	return cc->sendMessageBlock(&mb);
}

SubmissionMessage::SubmissionMessage() {
	_prob_id = 0;
	_user_id = 0;
	_time = 0;
	_server_id = 0;
	_content_size = 0;
	_content = 0;
	_language = "";
}

SubmissionMessage::SubmissionMessage(uint32_t prob_id, uint32_t user_id, const char* content, uint32_t content_size, std::string language) {
	_time = Server::contestTime();
	_user_id = user_id;
	_prob_id = prob_id;
	_server_id = Server::getId();
	_content_size = content_size;
	_content = new char[content_size];
	memcpy(_content, content, content_size);
	_language = language;
}

SubmissionMessage::~SubmissionMessage() {
	if(_content)
		delete []_content;
}

bool SubmissionMessage::process() const {
	DbCon* db = DbCon::getInstance();
	if(!db)
		return false;
	
	bool result = db->putSubmission(_user_id, _prob_id, _time, _server_id,
			_content, _content_size, _language);

	db->release();

	AttributeList lst = db->getProblemAttributes(_prob_id);
	std::string problem = lst["shortname"];
	
	MessageBlock mb("submission");
	if(result)
		mb["msg"] = "Your submission for '" + problem + "' has been accepted";
	else
		mb["msg"] = "Your sumission for '" + problem + "' has failed - please notify the contest administrator";

	EventRegister::getInstance().sendMessage(_user_id, &mb);
	
	// TODO: enqueue for marking if we are a marking server.

	return result;
}

uint16_t SubmissionMessage::message_type_id() const {
	return TYPE_ID_SUBMISSION;
}

uint32_t SubmissionMessage::storageRequired() {
	uint32_t required = 5 * sizeof(uint32_t);
	required += _content_size;
	required += _language.length() + 1;
	return required;
}

uint32_t SubmissionMessage::store(uint8_t* buffer, uint32_t size) {
	uint8_t *pos = buffer;
	*(uint32_t*)pos = _user_id; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _prob_id; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _time; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _server_id; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _content_size; pos += sizeof(uint32_t);
	memcpy(pos, _content, _content_size); pos += _content_size;
	strcpy((char*)pos, _language.c_str()); pos += _language.length() + 1;

	if((unsigned)(pos - buffer) > size)
		log(LOG_WARNING, "Buffer overflow detected - expect segfaults (Error is in class SubmissionMessage)");

	return pos - buffer;
}

uint32_t SubmissionMessage::load(const uint8_t* buffer, uint32_t size) {
	const uint8_t *pos = buffer;
	if(size < 5 * sizeof(uint32_t)) {
		log(LOG_ERR, "Too small buffer to contain the correct number of uint32_t values in SubmissionMessage::load()");
		return ~0U;
	}

	_user_id = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_prob_id = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_time = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_server_id = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_content_size = *(uint32_t*)pos; pos += sizeof(uint32_t);
	size -= 5 * sizeof(uint32_t);
	
	if(size < _content_size) {
		log(LOG_ERR, "Insufficient buffer data to load in SubmissionMessage::load()");
		return ~0U;
	}

	_content = new char[_content_size];
	memcpy(_content, pos, _content_size);
	pos += _content_size;
	size -= _content_size;

	if(!checkStringTerms(pos, size)) {
		log(LOG_ERR, "Incomplete string in buffer decoding");
		return ~0U;
	}

	_language = (char*)pos;
	pos += _language.length() + 1;

	return pos - buffer;
}

////////////////////////////////////////////////////////////////////
static Message* create_submission_msg() {
	return new SubmissionMessage();
}

static ActSubmit _act_submit;
static ActGetProblems _act_getproblems;
static ActGetSubmissions _act_getsubs;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "submit", &_act_submit);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "getproblems", &_act_getproblems);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "getsubmissions", &_act_getsubs);
	ClientAction::registerAction(USER_TYPE_ADMIN, "getsubmissions", &_act_getsubs);
	ClientAction::registerAction(USER_TYPE_JUDGE, "getsubmissions", &_act_getsubs);
	Message::registerMessageFunctor(TYPE_ID_SUBMISSION, create_submission_msg);
}
