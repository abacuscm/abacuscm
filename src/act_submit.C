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
#include "permissions.h"
#include "acmconfig.h"

#include <algorithm>
#include <sstream>
#include <vector>
#include <string>
#include <time.h>

using namespace std;

class ActSubmit : public ClientAction {
protected:
	virtual void int_process(ClientConnection *cc, MessageBlock *mb);
};

class ActGetProblems : public ClientAction {
protected:
	virtual void int_process(ClientConnection *cc, MessageBlock *mb);
};

class ActGetSubmissions : public ClientAction {
protected:
	virtual void int_process(ClientConnection *cc, MessageBlock *mb);
};

class ActGetSubmissionsForUser : public ClientAction {
protected:
	virtual void int_process(ClientConnection *cc, MessageBlock *mb);
};

class ActSubmissionFileFetcher : public ClientAction {
protected:
	virtual void int_process(ClientConnection *cc, MessageBlock *mb);
};

class ActGetSubmissionSource : public ClientAction {
protected:
	virtual void int_process(ClientConnection *cc, MessageBlock *mb);
};

class ActGetLanguages : public ClientAction {
private:
	vector<string> _languages;
protected:
	virtual void int_process(ClientConnection *cc, MessageBlock *mb);
public:
	ActGetLanguages();

	const vector<string> &getLanguages();
};

static ActSubmit _act_submit;
static ActGetProblems _act_getproblems;
static ActGetSubmissions _act_getsubs;
static ActGetSubmissionsForUser _act_getsubs_for_user;
static ActSubmissionFileFetcher _act_submission_file_fetcher;
static ActGetSubmissionSource _act_get_submission_source;
static ActGetLanguages _act_get_languages;

class SubmissionMessage : public Message {
private:
	uint32_t _submission_id;
	uint32_t _prob_id;
	uint32_t _user_id;
	uint32_t _time;
	uint32_t _server_id;
	uint32_t _content_size;
	char* _content;
	string _language;
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t* buffer, uint32_t size);
	virtual uint32_t load(const uint8_t* buffer, uint32_t size);

	virtual bool int_process() const;
public:
	SubmissionMessage();
	SubmissionMessage(uint32_t sub_id, uint32_t prob_id, uint32_t user_id, const char* content, uint32_t content_size, string language);
	virtual ~SubmissionMessage();

	virtual uint16_t message_type_id() const;
};

void ActSubmit::int_process(ClientConnection *cc, MessageBlock *mb) {
	if(getTimerSupportModule()->contestStatus(Server::getId()) != TIMER_STATUS_STARTED)
		return cc->sendError("You cannot submit solutions unless the contest is running");

	if(mb->content_size() > MAX_SUBMISSION_SIZE) {
		ostringstream msg;
		msg << "Your submission is too large (max " << MAX_SUBMISSION_SIZE << " bytes)";
		return cc->sendError(msg.str());
	}

	uint32_t user_id = cc->getUserId();
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
		if (db->getProblemAttributes(prob_id)["multi_submit"] != "Yes") {
			db->release(); db = NULL;
			return cc->sendError("You have already solved this problem, you may no longer submit solutions for it");
		}
	}

	if (!db->isSubmissionAllowed(user_id, prob_id))
		return cc->sendError("You are not allowed to submit a solution for this problem");

	db->release();db=NULL;

	string lang = (*mb)["lang"];
	const vector<string> &languages = _act_get_languages.getLanguages();
	if(find(languages.begin(), languages.end(), lang) == languages.end())
		return cc->sendError("You have not specified the language");

	uint32_t sub_id = Server::nextSubmissionId();
	if(sub_id == ~0U)
		return cc->sendError("Internal server error. Error obtaining new submission id");

	SubmissionMessage *msg = new SubmissionMessage(sub_id, prob_id, user_id,
			mb->content(), mb->content_size(), lang);

	log(LOG_INFO, "User %u submitted solution for problem %u", user_id, prob_id);

	triggerMessage(cc, msg);
}

void ActGetProblems::int_process(ClientConnection *cc, MessageBlock *) {
	uint32_t user_id = cc->getUserId();

	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Error connecting to database");

	ProblemList probs = db->getProblems();

	MessageBlock mb("ok");

	ProblemList::iterator p;
	int c = 0;
	for (p = probs.begin(); p != probs.end(); p++, c++) {
		uint32_t problem_id = *p;
		if (!cc->permissions()[PERMISSION_SEE_ALL_PROBLEMS] &&
			!db->isSubmissionAllowed(user_id, problem_id))
		{
			c--;
			continue;
		}

		ostringstream ostrstrm;
		ostrstrm << c;
		string cstr = ostrstrm.str();
		ostrstrm.str("");
		ostrstrm << *p;

		mb["id" + cstr] = ostrstrm.str();

		AttributeList lst = db->getProblemAttributes(*p);
		mb["code" + cstr] = lst["shortname"];
		mb["name" + cstr] = lst["longname"];
	}
	db->release();db=NULL;

	cc->sendMessageBlock(&mb);
}

void ActGetSubmissions::int_process(ClientConnection *cc, MessageBlock *) {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Error connecting to database");
	SubmissionSupportModule *submission = getSubmissionSupportModule();
	if (!submission)
		return cc->sendError("Error getting submission support module");

	uint32_t uid = cc->getUserId();

	SubmissionList lst;

	if(!cc->permissions()[PERMISSION_SEE_ALL_SUBMISSIONS])
		lst = db->getSubmissions(uid);
	else
		lst = db->getSubmissions();

	MessageBlock mb("ok");

	SubmissionList::iterator s;
	int c = 0;
	for(s = lst.begin(); s != lst.end(); ++s, ++c) {
		ostringstream tmp;
		tmp << c;
		string cntr = tmp.str();

		submission->submissionToMB(db, *s, mb, cntr);
	}
	db->release();db=NULL;

	cc->sendMessageBlock(&mb);
}

void ActGetSubmissionsForUser::int_process(ClientConnection *cc, MessageBlock *mb) {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Error connecting to database");
	SubmissionSupportModule *submission = getSubmissionSupportModule();
	if (!submission)
		return cc->sendError("Error getting submission support module");

	uint32_t uid = cc->getUserId();

	uint32_t user_id = strtoul((*mb)["user_id"].c_str(), NULL, 0);

	SubmissionList lst;

	if(!cc->permissions()[PERMISSION_SEE_ALL_SUBMISSIONS])
		lst = db->getSubmissions(uid);
	else
		lst = db->getSubmissions(user_id);

	MessageBlock result_mb("ok");

	SubmissionList::iterator s;
	int c = 0;
	for(s = lst.begin(); s != lst.end(); ++s, ++c) {
		ostringstream tmp;
		tmp << c;
		string cntr = tmp.str();

		submission->submissionToMB(db, *s, result_mb, cntr);
	}
	db->release();db=NULL;

	cc->sendMessageBlock(&result_mb);
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

SubmissionMessage::SubmissionMessage(uint32_t sub_id, uint32_t prob_id, uint32_t user_id, const char* content, uint32_t content_size, string language) {
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

bool SubmissionMessage::int_process() const {
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
			ClientEventRegistry::getInstance().broadcastEvent(
				_user_id, PERMISSION_SEE_ALL_SUBMISSIONS, &notify);

		}
		if(db->getServerAttribute(Server::getId(), "marker") == "yes")
			Markers::getInstance().enqueueSubmission(_submission_id);
	}

	if (!result) {
		AttributeList lst = db->getProblemAttributes(_prob_id);
		string problem = lst["shortname"];

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

void ActSubmissionFileFetcher::int_process(ClientConnection *cc, MessageBlock *mb) {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Error connecting to database");

	uint32_t uid = cc->getUserId();

	string request = (*mb)["request"];
	uint32_t submission_id = strtoll((*mb)["submission_id"].c_str(), NULL, 0);

	MessageBlock result_mb("ok");

	if (!cc->permissions()[PERMISSION_SEE_SUBMISSION_DETAILS]) {
		// make sure that this is a compilation failed type of error
		// and that the submission belongs to this contestant
		RunResult resinfo;
		uint32_t utype;
		string comment;

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
	}

	if (!cc->permissions()[PERMISSION_SEE_ALL_SUBMISSIONS]) {
		if (db->submission2user_id(submission_id) != uid) {
			db->release();db=NULL;
			return cc->sendError("This submission doesn't belong to you; I can't let you look at it");
		}
	}

	if (request == "count") {
		uint32_t count = db->countMarkFiles(submission_id);
		ostringstream str("");
		str << count;
		result_mb["count"] = str.str();
	}
	else if (request == "data") {
		uint32_t index = strtoll((*mb)["index"].c_str(), NULL, 0);
		string name;
		char *data;
		uint32_t length;
		bool result = db->getMarkFile(submission_id, index, name, &data, length);

		if (!result) {
			db->release(); db = NULL;
			log(LOG_ERR, "Failed to get submission file with index %u for submission_id %u\n", index, submission_id);
			return cc->sendError("Mark file not found");
		}

		result_mb["name"] = name;
		ostringstream str("");
		str << length;
		result_mb["length"] = str.str();
		result_mb.setContent((char *) data, length);
		delete []data;
	}

	db->release();db=NULL;

	cc->sendMessageBlock(&result_mb);
}

void ActGetSubmissionSource::int_process(ClientConnection *cc, MessageBlock *mb) {
	DbCon *db = DbCon::getInstance();
	if (!db)
		return cc->sendError("Error connecting to database");

	uint32_t submission_id = strtoll((*mb)["submission_id"].c_str(), NULL, 0);
	char* content;
	int length;
	string language;
	uint32_t prob_id;

	uint32_t uid = cc->getUserId();
	if (cc->permissions()[PERMISSION_SEE_ALL_SUBMISSIONS]) {
		uid = 0;
	}
	bool has_data = db->retrieveSubmission(uid, submission_id, &content, &length, language, &prob_id);
	db->release();db=NULL;

	if (!has_data)
		return cc->sendError("Unable to retrieve contestant source code");

	MessageBlock result_mb("ok");
	result_mb.setContent(content, length);

	cc->sendMessageBlock(&result_mb);
}

// Splits a comma-separated list into a vector
static vector<string> split_list(const string &s) {
	vector<string> ans;
	string::size_type start = 0;
	while (start < s.length()) {
		string::size_type end = s.find_first_of(",", start);
		if (end == string::npos)
			end = s.length();
		string item = s.substr(start, end - start);
		if (count(ans.begin(), ans.end(), item) != 0)
			log(LOG_WARNING, "Language '%s' listed twice, ignoring", item.c_str());
		else
			ans.push_back(item);
		start = end + 1;
	}
	return ans;
}

ActGetLanguages::ActGetLanguages() {
	const char * const all_languages = "C,C++,Python,Java";
	vector<string> all = split_list(all_languages);

	Config &conf = Config::getConfig();
	string language_set = conf["contest"]["languages"];
	if (language_set == "") {
		log(LOG_WARNING, "Languages are not set, defaulting to all");
		_languages = all;
	} else {
		_languages = split_list(language_set);
		size_t i = 0;
		while (i < _languages.size()) {
			if (find(all.begin(), all.end(), _languages[i]) == all.end()) {
				log(LOG_WARNING, "Ignoring unknown language '%s'", _languages[i].c_str());
				_languages.erase(_languages.begin() + i);
			} else
				i++;
		}
		sort(_languages.begin(), _languages.end());
	}
}

const vector<string> &ActGetLanguages::getLanguages() {
	return _languages;
}

void ActGetLanguages::int_process(ClientConnection *cc, MessageBlock *) {
	MessageBlock result_mb("ok");

	for (size_t i = 0; i < _languages.size(); i++) {
		ostringstream header;
		header << "language" << i;
		result_mb[header.str()] = _languages[i];
	}

	cc->sendMessageBlock(&result_mb);
}

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("submit",              PERMISSION_SUBMIT, &_act_submit);
	ClientAction::registerAction("getproblems",         PERMISSION_AUTH, &_act_getproblems);
	ClientAction::registerAction("getsubmissions",      PERMISSION_AUTH, &_act_getsubs);
	ClientAction::registerAction("getsubmissions_for_user",
		PERMISSION_SEE_ALL_SUBMISSIONS && PERMISSION_SEE_USER_ID, &_act_getsubs_for_user);
	ClientAction::registerAction("fetchfile",           PERMISSION_AUTH, &_act_submission_file_fetcher);
	ClientAction::registerAction("getsubmissionsource", PERMISSION_AUTH, &_act_get_submission_source);
	ClientAction::registerAction("getlanguages",        PERMISSION_AUTH, &_act_get_languages);
	Message::registerMessageFunctor(TYPE_ID_SUBMISSION, create_submission_msg);

	ClientEventRegistry::getInstance().registerEvent("updatesubmissions", PERMISSION_AUTH);
}
