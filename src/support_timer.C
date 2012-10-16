/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "timersupportmodule.h"
#include "dbcon.h"
#include "logger.h"
#include "acmconfig.h"
#include "scoped_lock.h"

#include <stdlib.h>
#include <sstream>

using std::ostringstream;

DEFINE_SUPPORT_MODULE(TimerSupportModule);

TimerSupportModule::TimerSupportModule()
{
	_evlist = NULL;
	pthread_mutex_init(&_writelock, NULL);
	pthread_mutex_init(&_group_state_lock, NULL);
}

TimerSupportModule::~TimerSupportModule()
{
	while(_evlist) {
		struct startstop_event* tmp = _evlist;
		_evlist = _evlist->next;
		delete tmp;
	}
	pthread_mutex_destroy(&_writelock);
	pthread_mutex_destroy(&_group_state_lock);
}

uint32_t TimerSupportModule::contestDuration()
{
	static bool warned = false;

	Config &conf = Config::getConfig();
	uint32_t duration = strtoul(conf["contest"]["duration"].c_str(), NULL, 0);
	if (duration == 0) {
		if (!warned) {
			log(LOG_WARNING, "Duration is NOT set explicitly, defaulting to 5 hours for backwards compatibility.");
			warned = true;
		}
		duration = 5 * 3600;
	} else if (duration < 1800 && !warned) {
		log(LOG_WARNING, "Duration is less than half an hour.  This is probably wrong.");
		warned = true;
	}
	return duration;
}

uint32_t TimerSupportModule::contestTime(uint32_t group_id, time_t real_time)
{
	uint32_t elapsed_time = 0;
	uint32_t last_start_time = 0;
	int status = TIMER_STATUS_STOPPED;

	if(!real_time)
		real_time = time(NULL);

	struct startstop_event* i;
	for(i = _evlist; i && i->time <= real_time; i = i->next) {
		if(!i->group_id || i->group_id == group_id) {
			if(i->action != status) {
				status = i->action;

				if(status == TIMER_STATUS_STARTED)
					last_start_time = i->time;
				else
					elapsed_time += i->time - last_start_time;
			}
		}
	}

	if(status == TIMER_STATUS_STARTED)
		elapsed_time += real_time - last_start_time;

	uint32_t duration = contestDuration();
	if(elapsed_time > duration)
		elapsed_time = duration;

	return elapsed_time;
}

int TimerSupportModule::contestStatus(uint32_t group_id, time_t real_time)
{
	int status = TIMER_STATUS_STOPPED;

	if(!real_time)
		real_time = time(NULL);

	struct startstop_event* i;
	for(i = _evlist; i && i->time <= real_time; i = i->next) {
		if(!i->group_id || i->group_id == group_id)
			status = i->action;
	}

	if(contestTime(group_id, real_time) >= contestDuration())
		status = TIMER_STATUS_STOPPED;

	log(LOG_DEBUG, "status=%d", status);
	return status;
}

time_t TimerSupportModule::contestEndTime(uint32_t group_id) {
	int status = TIMER_STATUS_STOPPED;
	time_t remaining = contestDuration();
	time_t last_start_time = 0;

	struct startstop_event* i;
	for(i = _evlist; i; i = i->next) {
		if(!i->group_id || i->group_id == group_id) {
			if(i->action != status) {
				status = i->action;
				if (status == TIMER_STATUS_STARTED)
					last_start_time = i->time;
				else {
					time_t elapsed = i->time - last_start_time;
					if (elapsed > remaining)
						return last_start_time + remaining;
					else
						remaining -= elapsed;
				}
			}
		}
	}
	if (status == TIMER_STATUS_STARTED)
		return last_start_time + remaining;
	else
		return 0;
}

std::vector<time_t> TimerSupportModule::allStartStopTimes() {
	std::vector<time_t> ans;
	struct startstop_event *i;
	for (i = _evlist; i; i = i->next) {
		ans.push_back(i->time);
	}
	return ans;
}

bool TimerSupportModule::scheduleStartStop(uint32_t group_id, time_t time, int action) {
	bool res = false;
	ostringstream query;

	query << "REPLACE INTO ContestStartStop(group_id, action, time) VALUES(" << group_id << ", '"
		<< (action == TIMER_START ? "START" : "STOP") << "', " << time << ")";

	pthread_mutex_lock(&_writelock);

	struct startstop_event **i = &_evlist;

	while(*i && (*i)->time < time)
		i = &(*i)->next;

	DbCon *db = DbCon::getInstance();
	if(!db) {
		log(LOG_ERR, "Unable to add scheduled start/stop due to lack of DB connectivity.");
		goto out;
	}

	res = db->executeQuery(query.str());
	db->release(); db = NULL;

	if (res) {
		struct startstop_event *t = new startstop_event;
		t->time = time;
		t->action = action;
		t->group_id = group_id;
		t->next = *i;
		*i = t;
	}
out:
	pthread_mutex_unlock(&_writelock);
	return res;
}

void TimerSupportModule::updateGroupState(uint32_t group_id, time_t time, int &old_state, int &new_state) {
	ScopedLock lock(&_group_state_lock);
	if (!_group_state.count(group_id)) {
		_group_state[group_id] = TIMER_STATUS_STOPPED;
	}
	old_state = _group_state[group_id];

	new_state = contestStatus(group_id, time);
	if (new_state != old_state)
		_group_state[group_id] = new_state;
}

void TimerSupportModule::init()
{
	DbCon *db = DbCon::getInstance();
	if(!db) {
		log(LOG_ERR, "Unable to load initial contest start/stop times from database.");
		return;
	}

	QueryResult res = db->multiRowQuery("SELECT group_id, action, time FROM ContestStartStop ORDER BY time DESC, group_id DESC");
	db->release(); db = NULL;

	QueryResult::iterator i = res.begin();
	for( ; i != res.end(); ++i) {
		struct startstop_event* ev = new struct startstop_event;

		ev->group_id = strtoul((*i)[0].c_str(), NULL, 0);
		ev->action = (*i)[1] == "START" ? TIMER_START : TIMER_STOP;
		ev->time = strtoul((*i)[2].c_str(), NULL, 0);
		ev->next = _evlist;

		_evlist = ev;

		log(LOG_DEBUG, "Initial timer event: group_id=%d, time=%lu, action=%d", ev->group_id, ev->time, ev->action);
	}
}
