#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "logger.h"
#include "markers.h"
#include "message.h"
#include "message_type_ids.h"
#include "server.h"
#include "dbcon.h"
#include "eventregister.h"

#include <string>
#include <list>
#include <regex.h>
#include <time.h>
#include <sstream>

using namespace std;

class ActSubscribeMark : public ClientAction {
protected:
	virtual bool int_process(ClientConnection*, MessageBlock* mb);
};

class ActPlaceMark : public ClientAction {
protected:
	virtual bool int_process(ClientConnection*, MessageBlock* mb);
};

class ActBalloon : public ClientAction {
protected:
	virtual bool int_process(ClientConnection*, MessageBlock* mb);
};

class MarkMessage : public Message {
private:
	struct File {
		std::string name;
		uint32_t len;
		uint8_t *data;
	};
	
	uint32_t _submission_id;
	uint32_t _result;
	uint32_t _time;
    uint32_t _marker;
    uint32_t _type;
	uint32_t _server_id;
	
	std::string _comment;
	std::list<File> _files;

protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);
public:
	MarkMessage();
	MarkMessage(uint32_t submission_id, uint32_t marker, uint32_t type, uint32_t result, std::string comment);
	virtual ~MarkMessage();

	virtual bool process() const;
	virtual uint16_t message_type_id() const;

	void addFile(std::string, uint32_t len, const void *data);
};

MarkMessage::MarkMessage() {
}

MarkMessage::MarkMessage(uint32_t submission_id, uint32_t marker, uint32_t type, uint32_t result, std::string comment) {
	_submission_id = submission_id;
    _marker = marker;
    _type = type;
	_result = result;
	_comment = comment;
	_server_id = Server::getId();
	_time = ::time(NULL);
}

MarkMessage::~MarkMessage() {
	list<File>::iterator i;
	for(i = _files.begin(); i != _files.end(); ++i)
		delete i->data;
}

void MarkMessage::addFile(std::string name, uint32_t len, const void *data) {
	File tmp;
	tmp.name = name;
	tmp.len = len;
	tmp.data = new uint8_t[len];
	memcpy(tmp.data, data, len);

	_files.push_back(tmp);
}

bool MarkMessage::process() const {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return false;

	if(!db->putMark(_submission_id, _marker, _time, _result, _comment, _server_id)) {
		db->release();
		return false;
	}
	
	list<File>::const_iterator i;
	for(i = _files.begin(); i != _files.end(); ++i) {
		if(!db->putMarkFile(_submission_id, _marker, i->name, i->data, i->len))
			log(LOG_WARNING, "An error occured placing a marker file into the database - continueing in any case");
	}

    if(_result != WRONG || _type != USER_TYPE_MARKER) {
        // in a non awaiting judge state
        MessageBlock mb("submission");
        mb["msg"] = "You have newly marked information available under submissions";

        EventRegister::getInstance().sendMessage(db->submission2user_id(_submission_id), &mb);
    }

    MessageBlock mb("submission");
    EventRegister::getInstance().triggerEvent("judgesubmission", &mb);

	if(_result == CORRECT) {
		MessageBlock st("standings");

		EventRegister::getInstance().broadcastEvent(&st);

		MessageBlock bl("balloon");
		bl["server"] = db->server_id2name(db->submission2server_id(_submission_id));
		bl["contestant"] = db->user_id2name(db->submission2user_id(_submission_id));
		bl["problem"] = db->submission2problem(_submission_id);

		EventRegister::getInstance().triggerEvent("balloon", &bl);
	}

	db->release();
	
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
	}

	if(size)
		log(LOG_NOTICE, "We haven't eaten the entire input buffer in MarkMessage::load!");
	return pos - buffer;
}

///////////////////////////////////////////////////////////
bool ActSubscribeMark::int_process(ClientConnection* cc, MessageBlock*) {
	Markers::getInstance().enqueueMarker(cc);
	return cc->reportSuccess();
}

bool ActPlaceMark::int_process(ClientConnection* cc, MessageBlock*mb) {
	std::string submission_id_str = (*mb)["submission_id"];
	char *errpnt;
	uint32_t submission_id = strtoll(submission_id_str.c_str(), &errpnt, 0);

	if(*errpnt || submission_id_str == "") {
		Markers::getInstance().preemptMarker(cc);
		return cc->sendError("Invalid submission_id");
	}

	if(cc->getProperty("user_type") == USER_TYPE_MARKER && Markers::getInstance().hasIssued(cc) != submission_id)
		return cc->sendError("Markers may only mark submissions issued to them");

    if (cc->getProperty("user_type") == USER_TYPE_JUDGE) {
        // extra code to avoid race conditions for judge marking
        RunResult resinfo;
        uint32_t utype;
        std::string comment;

        DbCon *db = DbCon::getInstance();
        if(!db)
            return cc->sendError("Error connecting to database");

        // POSSIBLE RACE BETWEEN DOING THIS CHECK AND ASSIGNING THE MARK
        // but it's very small & unlikely :-)
        if(db->getSubmissionState(
                                  submission_id,
                                  resinfo,
                                  utype,
                                  comment)) {
            db->release();
            if (utype == USER_TYPE_JUDGE) {
                return cc->sendError("A judge has already beat you to it and marked this submission, sorry!");
            }
            if (resinfo != WRONG) {
                return cc->sendError("You cannot change the status of this submission: the decision was black and white; no human required ;-)");
            }
        }
        else {
            db->release();
            return cc->sendError("This hasn't been compiled or even run: you really think I'm going to let you fiddle with the marks?");
        }
        db->release();
    }

	uint32_t result = strtoll((*mb)["result"].c_str(), &errpnt, 0);
	if(*errpnt || (*mb)["result"] == "") {
		Markers::getInstance().preemptMarker(cc);
		return cc->sendError("You must specify the result");
	}

	std::string comment = (*mb)["comment"];

	MarkMessage *markmsg = new MarkMessage(submission_id, cc->getProperty("user_id"), cc->getProperty("user_type"), result, comment);
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
		if(regexec(&file_reg, fdesc.c_str(), 4, m, 0)) {
			log(LOG_ERR, "Invalid file description for '%s' for mark submission for submission_id = %u", fdesc.c_str(), submission_id);
		} else {
			int start = strtoll(fdesc.substr(m[1].rm_so, m[1].rm_eo - m[1].rm_so).c_str(), NULL, 0);
			int size  = strtoll(fdesc.substr(m[2].rm_so, m[2].rm_eo - m[2].rm_so).c_str(), NULL, 0);
			string desc = fdesc.substr(m[3].rm_so, m[3].rm_eo - m[3].rm_so);

			markmsg->addFile(desc, size, mb->content() + start);
		}
	}

	Markers::getInstance().notifyMarked(cc, submission_id);
	return triggerMessage(cc, markmsg);
}

bool ActBalloon::int_process(ClientConnection* cc, MessageBlock* mb) {
	if((*mb)["action"] == "subscribe") {
		if(!EventRegister::getInstance().registerClient("balloon", cc))
			return cc->sendError("Error subscribing to balloon events.");
	} else {
		EventRegister::getInstance().deregisterClient("balloon", cc);
	}
	return cc->reportSuccess();
}

static ActSubscribeMark _act_subscribe_mark;
static ActPlaceMark _act_place_mark;
static ActBalloon _act_balloon;

static Message* create_mark_message() {
	return new MarkMessage();
}

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_MARKER, "subscribemark", &_act_subscribe_mark);
	ClientAction::registerAction(USER_TYPE_MARKER, "mark", &_act_place_mark);
	ClientAction::registerAction(USER_TYPE_JUDGE, "mark", &_act_place_mark);
	ClientAction::registerAction(USER_TYPE_JUDGE, "balloonnotify", &_act_balloon);
    Message::registerMessageFunctor(TYPE_ID_SUBMISSION_MARK, create_mark_message);
	EventRegister::getInstance().registerEvent("balloon");
}
