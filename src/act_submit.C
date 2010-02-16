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
#include "timersupportmodule.h"
#include "submissionsupportmodule.h"

#include <sstream>
#include <time.h>

class ActSubmit : public ClientAction {
protected:
	bool int_process(ClientConnection *cc, MessageBlock *mb);
};

class ActGetProblems : public ClientAction {
protected:
	bool int_process(ClientConnection *cc, MessageBlock *mb);
};

class ActGetSubmissibleProblems : public ClientAction {
protected:
	bool int_process(ClientConnection *cc, MessageBlock *mb);
};

class ActGetSubmissions : public ClientAction {
protected:
	bool int_process(ClientConnection *cc, MessageBlock *mb);
};

class ActSubmissionFileFetcher : public ClientAction {
protected:
	bool int_process(ClientConnection *cc, MessageBlock *mb);
};

class ActGetSubmissionSource : public ClientAction {
protected:
	bool int_process(ClientConnection *cc, MessageBlock *mb);
};

class SubmissionMessage : public Message {
private:
	uint32_t _submission_id;
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
	SubmissionMessage(uint32_t sub_id, uint32_t prob_id, uint32_t user_id, const char* content, uint32_t content_size, std::string language);
	virtual ~SubmissionMessage();

	virtual bool process() const;

	virtual uint16_t message_type_id() const;
};

bool ActSubmit::int_process(ClientConnection *cc, MessageBlock *mb) {
	if(getTimerSupportModule()->contestStatus(Server::getId()) != TIMER_STATUS_STARTED)
		return cc->sendError("You cannot submit solutions unless the contest is running");

	uint32_t user_id = cc->getProperty("user_id");
	char *errptr;
	uint32_t prob_id = strtol((*mb)["prob_id"].c_str(), &errptr, 0);
	if(*errptr || (*mb)["prob_id"] == "")
		return cc->sendError("prob_id isn't a valid integer");

	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Unable to connect to database");
	ProblemList probs = db->getProblems();

	ProblemList::iterator p;
	for(p = probs.begin(); p != probs.end(); ++p)
		if(*p == prob_id)
			break;
	if(p == probs.end()) {
		db->release();db=NULL;
		return cc->sendError("Invalid prob_id - no such id");
	}

	bool solved = db->hasSolved(user_id, prob_id);
	if(solved) {
		db->release(); db = NULL;
		return cc->sendError("You have already solved this problem, you may no longer submit solutions for it");
	}

	if (!db->isSubmissionAllowed(user_id, prob_id))
		return cc->sendError("You are not allowed to submit a solution for this problem");

	db->release();db=NULL;

	std::string lang = (*mb)["lang"];

	// TODO:  Proper check - this will do for now:
	if(lang != "C" && lang != "C++" && lang != "Java" && lang != "Python")
		return cc->sendError("You have not specified the language");

	uint32_t sub_id = Server::nextSubmissionId();
	if(sub_id == ~0U)
		return cc->sendError("Internal server error. Error obtaining new submission id");

	SubmissionMessage *msg = new SubmissionMessage(sub_id, prob_id, user_id,
			mb->content(), mb->content_size(), lang);

	log(LOG_INFO, "User %u submitted solution for problem %u", user_id, prob_id);

	return triggerMessage(cc, msg);
}

bool ActGetProblems::int_process(ClientConnection *cc, MessageBlock *) {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Error connecting to database");

	ProblemList probs = db->getProblems();

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
	db->release();db=NULL;

	return cc->sendMessageBlock(&mb);
}

bool ActGetSubmissibleProblems::int_process(ClientConnection *cc, MessageBlock *) {
	uint32_t user_id = cc->getProperty("user_id");
	uint32_t utype = cc->getProperty("user_type");

	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Error connecting to database");

	ProblemList probs = db->getProblems();

	MessageBlock mb("ok");

	ProblemList::iterator p;
	int c = 0;
	for (p = probs.begin(); p != probs.end(); p++, c++) {
		uint32_t problem_id = *p;
		if (utype != USER_TYPE_ADMIN && !db->isSubmissionAllowed(user_id, problem_id)) {
			c--;
			continue;
		}

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
	db->release();db=NULL;

	return cc->sendMessageBlock(&mb);
}

bool ActGetSubmissions::int_process(ClientConnection *cc, MessageBlock *) {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Error connecting to database");
	SubmissionSupportModule *submission = getSubmissionSupportModule();
	if (!submission)
		return cc->sendError("Error getting submission support module");

	uint32_t uid = cc->getProperty("user_id");
	uint32_t utype = cc->getProperty("user_type");

	SubmissionList lst;

	if(utype == USER_TYPE_CONTESTANT || utype == USER_TYPE_OBSERVER)
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

		submission->submissionToMB(db, *s, mb, cntr);
	}
	db->release();db=NULL;

	return cc->sendMessageBlock(&mb);
}

SubmissionMessage::SubmissionMessage() {
	_submission_id = 0;
	_prob_id = 0;
	_user_id = 0;
	_time = 0;
	_server_id = 0;
	_content_size = 0;
	_content = 0;
	_language = "";
}

SubmissionMessage::SubmissionMessage(uint32_t sub_id, uint32_t prob_id, uint32_t user_id, const char* content, uint32_t content_size, std::string language) {
	_submission_id = sub_id;
	_time = ::time(NULL);
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
	SubmissionSupportModule *submission = getSubmissionSupportModule();
	if(!submission)
		return false;

	DbCon* db = DbCon::getInstance();
	if(!db)
		return false;

	bool result = submission->putSubmission(_submission_id, _user_id, _prob_id, _time, _server_id,
			_content, _content_size, _language);

	if (result) {
		AttributeList s = db->getSubmission(_submission_id);
		if (s.empty())
			result = false;
		else {
			MessageBlock notify("updatesubmissions");
			submission->submissionToMB(db, s, notify, "");
			ClientEventRegistry::getInstance().broadcastEvent(_user_id, USER_MASK_JUDGE | USER_MASK_ADMIN, &notify);

		}
		if(db->getServerAttribute(Server::getId(), "marker") == "yes")
			Markers::getInstance().enqueueSubmission(_submission_id);
	}

	if (!result) {
		AttributeList lst = db->getProblemAttributes(_prob_id);
		std::string problem = lst["shortname"];

		MessageBlock mb("submission");
		mb["msg"] = "Your submission for '" + problem + "' has failed - please notify the contest administrator";

		ClientEventRegistry::getInstance().sendMessage(_user_id, &mb);
	}

	db->release();db=NULL;

	return result;
}

uint16_t SubmissionMessage::message_type_id() const {
	return TYPE_ID_SUBMISSION;
}

uint32_t SubmissionMessage::storageRequired() {
	uint32_t required = 6 * sizeof(uint32_t);
	required += _content_size;
	required += _language.length() + 1;
	return required;
}

uint32_t SubmissionMessage::store(uint8_t* buffer, uint32_t size) {
	uint8_t *pos = buffer;
	*(uint32_t*)pos = _submission_id; pos += sizeof(uint32_t);
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
	if(size < 6 * sizeof(uint32_t)) {
		log(LOG_ERR, "Too small buffer to contain the correct number of uint32_t values in SubmissionMessage::load()");
		return ~0U;
	}

	_submission_id = *(uint32_t*)pos; pos += sizeof(uint32_t);
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

bool ActSubmissionFileFetcher::int_process(ClientConnection *cc, MessageBlock *mb) {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Error connecting to database");

	uint32_t uid = cc->getProperty("user_id");
	uint32_t utype = cc->getProperty("user_type");

	std::string request = (*mb)["request"];
	uint32_t submission_id = strtoll((*mb)["submission_id"].c_str(), NULL, 0);

	MessageBlock result_mb("ok");

	if (utype == USER_TYPE_CONTESTANT || utype == USER_TYPE_OBSERVER) {
		// make sure that this is a compilation failed type of error
		// and that the submission belongs to this contestant
		RunResult resinfo;
		uint32_t utype;
		std::string comment;

		if(db->getSubmissionState(
								  submission_id,
								  resinfo,
								  utype,
								  comment)) {
			if (resinfo != COMPILE_FAILED) {
				db->release();db=NULL;
				return cc->sendError("Not allowed to fetch file data for anything except failed compilations");
			}
		}
		else {
			// no state => it hasn't been marked!
			db->release(); db = NULL;
			return cc->sendError("This submission hasn't been marked yet, please be patient :-)");
		}

		if (db->submission2user_id(submission_id) != uid) {
			db->release();db=NULL;
			return cc->sendError("This submission doesn't belong to you; I can't let you look at it");
		}
	}

	if (request == "count") {
		uint32_t count = db->countMarkFiles(submission_id);
		std::ostringstream str("");
		str << count;
		result_mb["count"] = str.str();
	}
	else if (request == "data") {
		uint32_t index = strtoll((*mb)["index"].c_str(), NULL, 0);
		std::string name;
		char *data;
		uint32_t length;
		bool result = db->getMarkFile(submission_id, index, name, &data, length);

		if (!result) {
			db->release(); db = NULL;
			log(LOG_ERR, "Failed to get submission file with index %u for submission_id %u\n", index, submission_id);
			return cc->sendError("Mark file not found");
		}

		result_mb["name"] = name;
		std::ostringstream str("");
		str << length;
		result_mb["length"] = str.str();
		result_mb.setContent((char *) data, length);
		delete []data;
	}

	db->release();db=NULL;

	return cc->sendMessageBlock(&result_mb);
}

bool ActGetSubmissionSource::int_process(ClientConnection *cc, MessageBlock *mb) {
	DbCon *db = DbCon::getInstance();
	if (!db)
		return cc->sendError("Error connecting to database");

	uint32_t submission_id = strtoll((*mb)["submission_id"].c_str(), NULL, 0);
	char* content;
	int length;
	std::string language;
	uint32_t prob_id;

	bool has_data = db->retrieveSubmission(submission_id, &content, &length, language, &prob_id);
	db->release();db=NULL;

	if (!has_data)
		return cc->sendError("Unable to retrieve contestant source code");

	MessageBlock result_mb("ok");
	result_mb.setContent(content, length);

	return cc->sendMessageBlock(&result_mb);
}

static ActSubmit _act_submit;
static ActGetProblems _act_getproblems;
static ActGetSubmissions _act_getsubs;
static ActSubmissionFileFetcher _act_submission_file_fetcher;
static ActGetSubmissionSource _act_get_submission_source;
static ActGetSubmissibleProblems _act_get_submissible_problems;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "submit", &_act_submit);
	ClientAction::registerAction(USER_TYPE_OBSERVER, "submit", &_act_submit);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "getproblems", &_act_getproblems);
	ClientAction::registerAction(USER_TYPE_OBSERVER, "getproblems", &_act_getproblems);
	ClientAction::registerAction(USER_TYPE_JUDGE, "getproblems", &_act_getproblems);
	ClientAction::registerAction(USER_TYPE_ADMIN, "getproblems", &_act_getproblems);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "getsubmissions", &_act_getsubs);
	ClientAction::registerAction(USER_TYPE_OBSERVER, "getsubmissions", &_act_getsubs);
	ClientAction::registerAction(USER_TYPE_ADMIN, "getsubmissions", &_act_getsubs);
	ClientAction::registerAction(USER_TYPE_JUDGE, "getsubmissions", &_act_getsubs);
	ClientAction::registerAction(USER_TYPE_JUDGE, "fetchfile", &_act_submission_file_fetcher);
	ClientAction::registerAction(USER_TYPE_ADMIN, "fetchfile", &_act_submission_file_fetcher);
	ClientAction::registerAction(USER_TYPE_OBSERVER, "fetchfile", &_act_submission_file_fetcher);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "fetchfile", &_act_submission_file_fetcher);
	ClientAction::registerAction(USER_TYPE_JUDGE, "getsubmissionsource", &_act_get_submission_source);
	ClientAction::registerAction(USER_TYPE_ADMIN, "getsubmissionsource", &_act_get_submission_source);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "getsubmissibleproblems", &_act_get_submissible_problems);
	ClientAction::registerAction(USER_TYPE_OBSERVER, "getsubmissibleproblems", &_act_get_submissible_problems);
	ClientAction::registerAction(USER_TYPE_JUDGE, "getsubmissibleproblems", &_act_get_submissible_problems);
	ClientAction::registerAction(USER_TYPE_ADMIN, "getsubmissibleproblems", &_act_get_submissible_problems);
	Message::registerMessageFunctor(TYPE_ID_SUBMISSION, create_submission_msg);

	ClientEventRegistry::getInstance().registerEvent("updatesubmissions");
}
