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
#include "message.h"
#include "message_type_ids.h"
#include "logger.h"
#include "server.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "dbcon.h"
#include "timedaction.h"
#include "eventregister.h"

#include <string>

using namespace std;

class ActStartStop : public ClientAction {
protected:
	virtual bool int_process(ClientConnection*, MessageBlock*);
};

class ActSubscribeTime : public ClientAction {
protected:
	virtual bool int_process(ClientConnection*, MessageBlock*);
};

#define ACT_STOP	1
#define ACT_START	2

class MsgStartStop : public Message {
private:
	uint32_t _server_id;
	uint32_t _time;
	uint32_t _action; // way too big...
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);
public:
	MsgStartStop();
	MsgStartStop(uint32_t server_id, uint32_t time, uint32_t action);

	virtual bool process() const;

	virtual uint16_t message_type_id() const;
};

class StartStopAction : public TimedAction {
private:
	uint32_t _action;
	/* This field is used to check that the DB hasn't changed in the
	 * meantime.
         */
        uint32_t _time;
public:
	StartStopAction(uint32_t time, uint32_t action);

	virtual void perform();
};

StartStopAction::StartStopAction(uint32_t time, uint32_t action)
	: TimedAction(time)
{
        _time = time;
	_action = action;
}

void StartStopAction::perform() {
	DbCon *db = DbCon::getInstance();
	uint32_t time = db->contestStartStopTime(Server::getId(), _action == ACT_START);
        db->release(); db = NULL;
	if (time == _time) {
		MessageBlock mb("startstop");
		mb["action"] = _action == ACT_START ? "start" : "stop";
		EventRegister::getInstance().triggerEvent("startstop", &mb);
	}
}

bool ActStartStop::int_process(ClientConnection* cc, MessageBlock *mb) {
	char *errpnt;

	uint32_t server_id;
	uint32_t time;
	uint32_t action;

	string t_action = (*mb)["action"];
	if(t_action == "start") {
		action = ACT_START;
	} else if(t_action == "stop") {
		action = ACT_STOP;
	} else
		return cc->sendError("Invalid 'action', must be either 'start' or 'stop'");

	time = strtoll((*mb)["time"].c_str(), &errpnt, 0);
	if(time == 0 || *errpnt)
		return cc->sendError("Invalid start/stop time specified");

	// note that an omitted or blank server_id translates to 0, meaning
        // "all" servers.
        string server_id_str = (*mb)["server_id"];
        if (server_id_str == "all" || server_id_str == "") server_id = 0;
        else if (server_id_str == "self") server_id = Server::getId();
        else
        {
                server_id = strtoll((*mb)["server_id"].c_str(), &errpnt, 0);
                if(*errpnt)
                        return cc->sendError("Invalid server_id specified");
        }

	return triggerMessage(cc, new MsgStartStop(server_id, time, action));
}

bool ActSubscribeTime::int_process(ClientConnection* cc, MessageBlock*) {
	if(EventRegister::getInstance().registerClient("startstop", cc)
	   && EventRegister::getInstance().registerClient("updateclock", cc))
		return cc->reportSuccess();
	else
		return cc->sendError("Unable to subscribe to the 'startstop' event");
}

MsgStartStop::MsgStartStop() {
}

MsgStartStop::MsgStartStop(uint32_t server_id, uint32_t time, uint32_t action) {
	_server_id = server_id;
	_time = time;
	_action = action;
}

uint32_t MsgStartStop::storageRequired() {
	return 3 * sizeof(uint32_t);
}

uint32_t MsgStartStop::store(uint8_t *buffer, uint32_t size) {
	if(size < 3 * sizeof(uint32_t))
		return ~0U;

	uint32_t *pos = (uint32_t*)buffer;
	*pos++ = _server_id;
	*pos++ = _time;
	*pos++ = _action;
	return 3 * sizeof(uint32_t);
}

uint32_t MsgStartStop::load(const uint8_t *buffer, uint32_t size) {
	if(size < 3 * sizeof(uint32_t))
		return ~0U;

	const uint32_t *pos = (const uint32_t*)buffer;
	_server_id = *pos++;
	_time = *pos++;
	_action = *pos++;

	log(LOG_DEBUG, "MsgStartStop: %d %d %d", _server_id, _time, _action);
	return 3 * sizeof(uint32_t);
}

bool MsgStartStop::process() const {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return false;

	uint32_t now = ::time(NULL);
	bool oldRunning = db->contestRunning(Server::getId(), now);
	uint32_t oldTime = db->contestStartStopTime(Server::getId(), _action == ACT_START);
	bool dbres = db->startStopContest(_server_id, _time, _action == ACT_START);
	bool newRunning = db->contestRunning(Server::getId(), now);
	uint32_t newTime = db->contestStartStopTime(Server::getId(), _action == ACT_START);
	db->release();db=NULL;
        if (!dbres) return false;

	if (oldRunning != newRunning)
	{
		MessageBlock mb("startstop");
		mb["action"] = newRunning ? "start" : "stop";
		EventRegister::getInstance().triggerEvent("startstop", &mb);
	}
	if (oldTime != newTime && newTime >= now)
	{
		Server::putTimedAction(new StartStopAction(newTime, _action));
		MessageBlock mb("updateclock");
		EventRegister::getInstance().triggerEvent("updateclock", &mb);
	}

	return true;
}

uint16_t MsgStartStop::message_type_id() const {
	return TYPE_ID_STARTSTOP;
}

static void initialise_startstop_events() {
	DbCon *db = DbCon::getInstance();

	if(!db) {
		log(LOG_CRIT, "Unable to initialize start/stop times");
		return;
	}

	time_t start = db->contestStartStopTime(Server::getId(), true);
	time_t stop = db->contestStartStopTime(Server::getId(), false);
	db->release();
	time_t now = time(NULL);

	if (start > now)
	{
		Server::putTimedAction(new StartStopAction(start, ACT_START));
		log(LOG_INFO, "Setting start alarm for %u", (unsigned) start);
	}
	if (stop > now)
	{
		Server::putTimedAction(new StartStopAction(stop, ACT_STOP));
		log(LOG_INFO, "Setting stop alarm for %u", (unsigned) stop);
	}
}

static ActStartStop _act_startstop;
static ActSubscribeTime _act_subscribetime;

static Message* StartStopFunctor() {
	return new MsgStartStop();
}

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "startstop", &_act_startstop);
	ClientAction::registerAction(USER_TYPE_ADMIN, "subscribetime", &_act_subscribetime);
	ClientAction::registerAction(USER_TYPE_JUDGE, "subscribetime", &_act_subscribetime);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "subscribetime", &_act_subscribetime);
	Message::registerMessageFunctor(TYPE_ID_STARTSTOP, StartStopFunctor);
	EventRegister::getInstance().registerEvent("startstop");
	EventRegister::getInstance().registerEvent("updateclock");

	initialise_startstop_events();
}
