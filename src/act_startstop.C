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
#include "clienteventregistry.h"
#include "timersupportmodule.h"
#include "standingssupportmodule.h"
#include "usersupportmodule.h"

#include <string>
#include <memory>
#include <map>
#include <set>

using namespace std;

class ActStartStop : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection*, const MessageBlock*);
};

class ActSubscribeTime : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection*, const MessageBlock*);
};

class MsgStartStop : public Message {
private:
	uint32_t _group_id;
	uint32_t _time;
	uint32_t _action; // way too big...
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);

	virtual bool int_process() const;
public:
	MsgStartStop();
	MsgStartStop(uint32_t group_id, uint32_t time, uint32_t action);

	virtual uint16_t message_type_id() const;
};

/**
 * Executing this action simply checks the state and notifies any
 * clients that need to be notified of changes. Due to corner cases
 * some of them may end up doing nothing, but those are harmless.
 */
class StartStopAction : public TimedAction {
public:
	explicit StartStopAction(uint32_t time);

	virtual void perform();
};

StartStopAction::StartStopAction(uint32_t time)
: TimedAction(time)
{
}

void StartStopAction::perform() {
	UserSupportModule *usm = getUserSupportModule();
	if (!usm) {
		log(LOG_WARNING, "Could not get user support module");
		return;
	}
	TimerSupportModule *timer = getTimerSupportModule();

	time_t time = processingTime();
	// TODO: would be more efficient to query only the IDs. Could also
	// store a group in the StartStopAction and only loop when there is
	// the all group.
	UserSupportModule::GroupList groups = usm->groupList();
	for (UserSupportModule::GroupList::const_iterator i = groups.begin(); i != groups.end(); ++i) {
		int old_state, new_state;
		timer->updateGroupState(i->group_id, time, old_state, new_state);
		if (old_state != new_state) {
			MessageBlock mb("startstop");
			mb["action"] = new_state == TIMER_STATUS_STARTED ? "start" : "stop";
			ClientEventRegistry::getInstance().triggerGroupEvent("startstop", &mb, i->group_id);
		}
	}
}

auto_ptr<MessageBlock> ActStartStop::int_process(ClientConnection* cc, const MessageBlock *mb) {
	char *errpnt;

	uint32_t group_id;
	uint32_t time;
	uint32_t action;

	string t_action = (*mb)["action"];
	if(t_action == "start") {
		action = TIMER_START;
	} else if(t_action == "stop") {
		action = TIMER_STOP;
	} else
		return MessageBlock::error("Invalid 'action', must be either 'start' or 'stop'");

	time = strtoll((*mb)["time"].c_str(), &errpnt, 0);
	if(time == 0 || *errpnt)
		return MessageBlock::error("Invalid start/stop time specified");

	// note that an omitted or blank group_id translates to 0, meaning
	// all groups.
	string group_id_str = (*mb)["group_id"];
	if (group_id_str == "all" || group_id_str == "")
		group_id = 0;
	else
	{
		group_id = strtoll((*mb)["group_id"].c_str(), &errpnt, 0);
		if(*errpnt)
			return MessageBlock::error("Invalid group_id specified");
	}

	return triggerMessage(cc, new MsgStartStop(group_id, time, action));
}

auto_ptr<MessageBlock> ActSubscribeTime::int_process(ClientConnection* cc, const MessageBlock*) {
	if(ClientEventRegistry::getInstance().registerClient("startstop", cc)
			&& ClientEventRegistry::getInstance().registerClient("updateclock", cc))
		return MessageBlock::ok();
	else
		return MessageBlock::error("Unable to subscribe to the 'startstop' event");
}

MsgStartStop::MsgStartStop() {
}

MsgStartStop::MsgStartStop(uint32_t group_id, uint32_t time, uint32_t action) {
	_group_id = group_id;
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
	*pos++ = _group_id;
	*pos++ = _time;
	*pos++ = _action;
	return 3 * sizeof(uint32_t);
}

uint32_t MsgStartStop::load(const uint8_t *buffer, uint32_t size) {
	if(size < 3 * sizeof(uint32_t))
		return ~0U;

	const uint32_t *pos = (const uint32_t*)buffer;
	_group_id = *pos++;
	_time = *pos++;
	_action = *pos++;

	log(LOG_DEBUG, "MsgStartStop: %d %d %d", _group_id, _time, _action);
	return 3 * sizeof(uint32_t);
}

bool MsgStartStop::int_process() const {
	time_t now = ::time(NULL);
	bool future = _time > now;

	TimerSupportModule *timer = getTimerSupportModule();
	if(!timer)
		return false;
	UserSupportModule *usm = getUserSupportModule();
	if (!usm)
		return false;

	DbCon *db = DbCon::getInstance();
	if(!db)
		return false;

	bool dbres = timer->scheduleStartStop(_group_id, _time, _action);
	db->release();db=NULL;
	if (!dbres) return false;

	if (future) {
		Server::putTimedAction(new StartStopAction(_time));
	}
	else {
		// Might need to tell some groups of change immediately
		StartStopAction act(now);
		act.perform();

		// Contest time may have changed, which can invalidate standings and
		// the clock
		MessageBlock mb("updateclock");
		if (_group_id == 0)
			ClientEventRegistry::getInstance().triggerEvent("updateclock", &mb);
		else
			ClientEventRegistry::getInstance().triggerGroupEvent("updateclock", &mb, _group_id);

		StandingsSupportModule *standings = getStandingsSupportModule();
		standings->updateStandings(0, 0);
	}

	/* The change may have caused the implicit stop time to change for some groups. */
	UserSupportModule::GroupList groups = usm->groupList();
	set<time_t> done;
	for (UserSupportModule::GroupList::const_iterator i = groups.begin(); i != groups.end(); ++i) {
		if (_group_id == 0 || _group_id == i->group_id) {
			time_t end = timer->contestEndTime(i->group_id);
			if (end > now) {
				if (done.insert(end).second) // new to the set
					Server::putTimedAction(new StartStopAction(end));
			}
		}
	}

	return true;
}

uint16_t MsgStartStop::message_type_id() const {
	return TYPE_ID_STARTSTOP;
}

static void initialise_startstop_events() {
	TimerSupportModule *timer = getTimerSupportModule();
	if(!timer) {
		log(LOG_CRIT, "Unable to initialize start/stop times");
		return;
	}
	UserSupportModule *usm = getUserSupportModule();
	if (!usm) {
		log(LOG_CRIT, "Unable to find user support module");
		return;
	}

	time_t now = ::time(NULL);
	UserSupportModule::GroupList groups = usm->groupList();

	for (UserSupportModule::GroupList::const_iterator i = groups.begin(); i != groups.end(); ++i) {
		int old_state, new_state;
		timer->updateGroupState(i->group_id, now, old_state, new_state);
		time_t end = timer->contestEndTime(i->group_id);
		if (end > now)
			Server::putTimedAction(new StartStopAction(end));
	}

	std::vector<time_t> events = timer->allStartStopTimes();
	for (unsigned int i = 0; i < events.size(); i++)
		if (events[i] > now)
			Server::putTimedAction(new StartStopAction(events[i]));
}

static ActStartStop _act_startstop;
static ActSubscribeTime _act_subscribetime;

static Message* StartStopFunctor() {
	return new MsgStartStop();
}

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("startstop", PERMISSION_START_STOP, &_act_startstop);
	ClientAction::registerAction("subscribetime", PERMISSION_AUTH, &_act_subscribetime);
	Message::registerMessageFunctor(TYPE_ID_STARTSTOP, StartStopFunctor);
	ClientEventRegistry::getInstance().registerEvent("startstop", PERMISSION_AUTH);
	ClientEventRegistry::getInstance().registerEvent("updateclock", PERMISSION_AUTH);

	initialise_startstop_events();
}
