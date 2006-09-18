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

#include <stdlib.h>
#include <sstream>

using std::ostringstream;

TimerSupportModule::TimerSupportModule()
{
	_evlist = NULL;
	pthread_mutex_init(&_writelock, NULL);
}

TimerSupportModule::~TimerSupportModule()
{
	while(_evlist) {
		struct startstop_event* tmp = _evlist;
		_evlist = _evlist->next;
		delete tmp;
	}
	pthread_mutex_destroy(&_writelock);
}

uint32_t TimerSupportModule::contestDuration()
{
	// TODO: read this from an attribute and cache it.
	return 5 * 60 * 60; // 5 hours.
}

uint32_t TimerSupportModule::contestTime(uint32_t server_id, time_t real_time)
{
	uint32_t elapsed_time = 0;
	uint32_t last_start_time = 0;
	int status = TIMER_STATUS_STOPPED;

	if(!real_time)
		real_time = time(NULL);

	struct startstop_event* i;
	for(i = _evlist; i && i->time <= real_time; i = i->next) {
		if(!i->server_id || i->server_id == server_id) {
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

int TimerSupportModule::contestStatus(uint32_t server_id, time_t real_time)
{
	int status = TIMER_STATUS_STOPPED;

	if(!real_time)
		real_time = time(NULL);

	struct startstop_event* i;
	for(i = _evlist; i && i->time <= real_time; ++i) {
		if(!i->server_id || i->server_id == server_id)
			status = i->action;
	}

	if(contestTime(server_id, real_time) >= contestDuration())
		status = TIMER_STATUS_STOPPED;

	log(LOG_DEBUG, "status=%d", status);
	return status;
}

bool TimerSupportModule::nextScheduledStartStopAfter(uint32_t server_id, time_t after_time, time_t *next_time, int *next_action) {
	if (!after_time)
		after_time = time(NULL);

	struct startstop_event* i = _evlist;
	while (i && i->time < after_time)
		i = i->next;

	while (i && i->server_id && i->server_id != server_id)
		i = i-> next;

	if (i) {
		if (next_time)
			*next_time = i->time;
		if (next_action)
			*next_action = i->action;

		return true;
	}

	uint32_t duration = contestDuration();
	uint32_t contesttime = contestTime(server_id, after_time);

	if(contesttime < duration) {
		if (next_time)
			*next_time = after_time + duration - contesttime;
		if (next_action)
			*next_action = TIMER_STOP;
		return true;
	}

	return false;
}

bool TimerSupportModule::scheduleStartStop(uint32_t server_id, time_t time, int action) {
	bool res = false;
	ostringstream query;

	query << "INSERT INTO ContestStartStop(server_id, action, time) VALUES(" << server_id << ", "
		<< time << ", '" << (action == TIMER_START ? "START" : "STOP") << "')";

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
		t->server_id = server_id;
		t->next = *i;
		*i = t;
	}
out:
	pthread_mutex_unlock(&_writelock);
	return res;
}

void TimerSupportModule::init()
{
	DbCon *db = DbCon::getInstance();
	if(!db) {
		log(LOG_ERR, "Unable to load initial contest start/stop times from database.");
		return;
	}

	QueryResult res = db->multiRowQuery("SELECT server_id, action, time FROM ContestStartStop ORDER BY time DESC, server_id DESC");
	db->release(); db = NULL;

	QueryResult::iterator i = res.begin();
	for( ; i != res.end(); ++i) {
		struct startstop_event* ev = new struct startstop_event;

		ev->server_id = strtoul((*i)[0].c_str(), NULL, 0);
		ev->action = (*i)[1] == "START" ? TIMER_START : TIMER_STOP;
		ev->time = strtoul((*i)[2].c_str(), NULL, 0);
		ev->next = _evlist;

		_evlist = ev;

		log(LOG_DEBUG, "Initial timer event: server_id=%d, time=%lu, action=%d", ev->server_id, ev->time, ev->action);
	}
}

DEFINE_SUPPORT_MODULE_REGISTRAR(TimerSupportModule);
