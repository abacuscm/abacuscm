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
#include "messageblock.h"
#include "logger.h"
#include "markers.h"
#include "message.h"
#include "message_type_ids.h"
#include "server.h"
#include "dbcon.h"
#include "clienteventregistry.h"
#include "standingssupportmodule.h"
#include "usersupportmodule.h"
#include "submissionsupportmodule.h"
#include "timersupportmodule.h"
#include "permissionmap.h"

#include <string>
#include <list>
#include <regex.h>
#include <time.h>
#include <sstream>
#include <memory>

using namespace std;

class ActSubscribeMark : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection*, const MessageBlock* mb);
};

class ActPlaceMark : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection*, const MessageBlock* mb);
};

class MarkMessage : public Message {
private:
	struct File {
		string name;
		uint32_t len;
		uint8_t *data;
	};

	uint32_t _submission_id;
	uint32_t _result;
	uint32_t _time;
	uint32_t _marker;
	uint32_t _server_id;

	string _comment;
	list<File> _files;

protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);

	virtual bool int_process() const;
public:
	MarkMessage();
	MarkMessage(uint32_t submission_id, uint32_t marker, uint32_t result, string comment);
	virtual ~MarkMessage();

	virtual uint16_t message_type_id() const;

	void addFile(string, uint32_t len, const void *data);
};

MarkMessage::MarkMessage() {
}

MarkMessage::MarkMessage(uint32_t submission_id, uint32_t marker, uint32_t result, string comment) {
	_submission_id = submission_id;
	_marker = marker;
	_result = result;
	_comment = comment;
	_server_id = Server::getId();
	_time = ::time(NULL);
}

MarkMessage::~MarkMessage() {
	list<File>::iterator i;
	for(i = _files.begin(); i != _files.end(); ++i)
		delete []i->data;
}

void MarkMessage::addFile(string name, uint32_t len, const void *data) {
	File tmp;
	tmp.name = name;
	tmp.len = len;
	tmp.data = new uint8_t[len];
	memcpy(tmp.data, data, len);

	_files.push_back(tmp);
}

bool MarkMessage::int_process() const {
	UserSupportModule* usm = getUserSupportModule();
	if (!usm)
		return false;
	SubmissionSupportModule *submission = getSubmissionSupportModule();
	if (!submission)
		return false;

	DbCon *db = DbCon::getInstance();
	if(!db)
		return false;

	if(!db->putMark(_submission_id, _marker, _time, _result, _comment, _server_id)) {
		db->release();db=NULL;
		return false;
	}

	list<File>::const_iterator i;
	for(i = _files.begin(); i != _files.end(); ++i) {
		if(!db->putMarkFile(_submission_id, _marker, i->name, i->data, i->len))
			log(LOG_WARNING, "An error occured placing a marker file into the database - continueing in any case");
	}


	MessageBlock mb("updatesubmissions");
	AttributeList s = db->getSubmission(_submission_id);
	submission->submissionToMB(db, s, mb, "");

	uint32_t user_id = db->submission2user_id(_submission_id);
	uint32_t group_id = usm->user_group(user_id);
	ClientEventRegistry::getInstance().broadcastEvent(
		user_id, PERMISSION_SEE_ALL_SUBMISSIONS, &mb);

	StandingsSupportModule *standings = getStandingsSupportModule();
	if(standings && _result != JUDGE)
		standings->updateStandings(user_id, 0);

	TimerSupportModule *timer = getTimerSupportModule();

	if (_result == CORRECT && timer && !timer->isBlinded(timer->contestTime(group_id, strtoul(s["time"].c_str(), NULL, 0)))) {
		MessageBlock bl("balloon");
		bl["server"] = Server::servername(db->submission2server_id(_submission_id));
		bl["contestant"] = usm->username(user_id);
		bl["problem"] = mb["problem"];
		bl["group"] = usm->groupname(group_id);

		ClientEventRegistry::getInstance().triggerEvent("balloon", &bl);
	}

	db->release();db=NULL;

	return true;
}

uint16_t MarkMessage::message_type_id() const {
		return TYPE_ID_SUBMISSION_MARK;
}

uint32_t MarkMessage::storageRequired() {
	uint32_t required = 0;

	required += sizeof(uint32_t) * 5;
	required += _comment.length() + 1;

	list<File>::iterator i;
	for(i = _files.begin(); i != _files.end(); ++i)
		required += i->name.length() + 1 +
			sizeof(i->len) + i->len;

	return required;
}

uint32_t MarkMessage::store(uint8_t *buffer, uint32_t size) {
	uint8_t *pos = buffer;
	*(uint32_t*)pos = _submission_id; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _marker; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _result; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _time; pos += sizeof(uint32_t);
	*(uint32_t*)pos = _server_id; pos += sizeof(uint32_t);
	strcpy((char*)pos, _comment.c_str()); pos += strlen((char*)pos) + 1;

	list<File>::iterator i;
	for(i = _files.begin(); i != _files.end(); ++i) {
		strcpy((char*)pos, i->name.c_str()); pos += strlen((char*)pos) + 1;
		*(uint32_t*)pos = i->len; pos += sizeof(uint32_t);
		memcpy(pos, i->data, i->len); pos += i->len;
	}

	if((uint32_t)(pos - buffer) > size)
		log(LOG_ERR, "Buffer overflow detected - expect SEGFAULTS");

	return pos - buffer;
}

uint32_t MarkMessage::load(const uint8_t *buffer, uint32_t size) {
	const uint8_t *pos = buffer;
	if(size < sizeof(uint32_t) * 5) {
		log(LOG_ERR, "Insufficient data in input buffer.");
		return ~0U;
	}

	_submission_id = *(uint32_t*)pos; pos += sizeof(uint32_t); size -= sizeof(uint32_t);
	_marker = *(uint32_t*)pos; pos += sizeof(uint32_t); size -= sizeof(uint32_t);
	_result = *(uint32_t*)pos; pos += sizeof(uint32_t); size -= sizeof(uint32_t);
	_time = *(uint32_t*)pos; pos += sizeof(uint32_t); size -= sizeof(uint32_t);
	_server_id = *(uint32_t*)pos; pos += sizeof(uint32_t); size -= sizeof(uint32_t);

	if(!checkStringTerms(pos, size, 1)) {
		log(LOG_ERR, "Insufficient data in input buffer.");
		return ~0U;
	}

	_comment = (char*)pos; size -= strlen((char*)pos) + 1; pos += strlen((char*)pos) + 1;

	while(size > sizeof(int32_t) && checkStringTerms(pos, size - sizeof(int32_t), 1)) {
		File tmp;
		tmp.name = (char*)pos;
		pos += tmp.name.length() + 1;
		size -= tmp.name.length() + 1;

		tmp.len = *(uint32_t*)pos; pos += sizeof(uint32_t); size -= sizeof(uint32_t);

		if(size < tmp.len) {
			log(LOG_ERR, "Unsufficient data in input buffer.");
			return ~0U;
		}

		tmp.data = new uint8_t[tmp.len];
		memcpy(tmp.data, pos, tmp.len);

		pos += tmp.len;
		size -= tmp.len;

		_files.push_back(tmp);
	}

	if(size)
		log(LOG_NOTICE, "We haven't eaten the entire input buffer in MarkMessage::load!");
	return pos - buffer;
}

///////////////////////////////////////////////////////////
auto_ptr<MessageBlock> ActSubscribeMark::int_process(ClientConnection* cc, const MessageBlock*) {
	Markers::getInstance().enqueueMarker(cc);
	return MessageBlock::ok();
}

auto_ptr<MessageBlock> ActPlaceMark::int_process(ClientConnection* cc, const MessageBlock*mb) {
	string submission_id_str = (*mb)["submission_id"];
	char *errpnt;
	uint32_t submission_id = strtoll(submission_id_str.c_str(), &errpnt, 0);

	if (*errpnt || submission_id_str == "") {
		Markers::getInstance().preemptMarker(cc);
		return MessageBlock::error("Invalid submission_id");
	}

	/* Permission checks. As soon as any validity condition is found, legal
	 * is set to true. If a permission is found but some other condition
	 * prevents it from making the mark legal, msg is set to an explanation.
	 * It is possible to msg to be set multiple times, but that can only
	 * happen when a user has both PERMISSION_MARK and PERMISSION_JUDGE, which
	 * is not expected. It is also possible to msg to be set and legal to later
	 * be set to true.
	 */
	bool legal = false;
	string msg = "";

	if (cc->permissions()[PERMISSION_JUDGE_OVERRIDE]) {
		legal = true;
	}
	else if (cc->permissions()[PERMISSION_MARK]) {
		if (Markers::getInstance().hasIssued(cc) == submission_id)
		{
			legal = true;
		}
		else
		{
			msg = "Markers may only mark submissions issued to them";
		}
	}
	else if (cc->permissions()[PERMISSION_JUDGE]) {
		// extra code to avoid race conditions for judge marking
		RunResult resinfo;
		uint32_t utype;
		string comment;

		DbCon *db = DbCon::getInstance();
		if(!db)
			return MessageBlock::error("Error connecting to database");

		// POSSIBLE RACE BETWEEN DOING THIS CHECK AND ASSIGNING THE MARK
		// but it's very small & unlikely ... and doesn't really affect anything.
		bool res = db->getSubmissionState( submission_id, resinfo, utype, comment);
		const PermissionSet &uperms = PermissionMap::getInstance()->getPermissions(static_cast<UserType>(utype));
		db->release();db=NULL;

		if (!res) {
			msg = "This hasn't been compiled or even run: you really think I'm going to let you fiddle with the marks?";
		}
		else if (uperms[PERMISSION_JUDGE]) {
			msg = "Another judge has already this submission, sorry!";
		}
		else if (resinfo != JUDGE) {
			msg = "You cannot change the status of this submission: the decision was black and white; no human required.";
		}
		else {
			legal = true;
		}
	}

	if (!legal) {
		if (msg == "") {
			msg = "You do not have permission to do this";
		}
		return MessageBlock::error(msg);
	}

	uint32_t result = strtoll((*mb)["result"].c_str(), &errpnt, 0);
	if(*errpnt || (*mb)["result"] == "") {
		Markers::getInstance().preemptMarker(cc);
		return MessageBlock::error("You must specify the result");
	}

	string comment = (*mb)["comment"];

	MarkMessage *markmsg = new MarkMessage(submission_id, cc->getUserId(), result, comment);
	int c = 0;
	regex_t file_reg;
	regcomp(&file_reg, "^([0-9]{1,7}) ([0-9]{1,7}) ([[:print:]]+)$", REG_EXTENDED);
	while(true) {
		ostringstream ostrstrm;
		ostrstrm << "file" << c++;
		if(!mb->hasAttribute(ostrstrm.str()))
			break;

		string fdesc = (*mb)[ostrstrm.str()];
		regmatch_t m[4];
		if (regexec(&file_reg, fdesc.c_str(), 4, m, 0)) {
			log(LOG_ERR, "Invalid file description for '%s' for mark submission for submission_id = %u", fdesc.c_str(), submission_id);
		} else {
			int start = strtoll(fdesc.substr(m[1].rm_so, m[1].rm_eo - m[1].rm_so).c_str(), NULL, 0);
			int size  = strtoll(fdesc.substr(m[2].rm_so, m[2].rm_eo - m[2].rm_so).c_str(), NULL, 0);
			string desc = fdesc.substr(m[3].rm_so, m[3].rm_eo - m[3].rm_so);

			markmsg->addFile(desc, size, mb->content() + start);
		}
	}
	regfree(&file_reg);

	Markers::getInstance().notifyMarked(cc, submission_id);
	return triggerMessage(cc, markmsg);
}

static ActSubscribeMark _act_subscribe_mark;
static ActPlaceMark _act_place_mark;

static Message* create_mark_message() {
	return new MarkMessage();
}

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("subscribemark", PERMISSION_MARK, &_act_subscribe_mark);
	ClientAction::registerAction("mark",
		PERMISSION_MARK || PERMISSION_JUDGE, &_act_place_mark);
	Message::registerMessageFunctor(TYPE_ID_SUBMISSION_MARK, create_mark_message);
	ClientEventRegistry::getInstance().registerEvent(
		"balloon",
		PERMISSION_SEE_STANDINGS);
}
