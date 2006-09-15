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

TimerSupportModule::TimerSupportModule()
{
}

TimerSupportModule::~TimerSupportModule()
{
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

	startstop_event_list::const_iterator i;
	for(i = _evlist.begin(); i != _evlist.end() && i->time <= real_time; ++i) {
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

	startstop_event_list::const_iterator i;
	for(i = _evlist.begin(); i != _evlist.end() && i->time <= real_time; ++i) {
		if(!i->server_id || i->server_id == server_id)
			status = i->action;
	}

	return status;
}

void TimerSupportModule::init()
{
	DbCon *db = DbCon::getInstance();
	if(!db) {
		log(LOG_ERR, "Unable to load initial contest start/stop times from database.");
		return;
	}

	QueryResult res = db->multiRowQuery("SELECT server_id, action, time FROM ContestStartStop ORDER BY time, server_id");

	QueryResult::iterator i = res.begin();
	for( ; i != res.end(); ++i) {
		startstop_event ev;

		ev.server_id = strtoul((*i)[0].c_str(), NULL, 0);
		ev.action = (*i)[1] == "START" ? TIMER_START : TIMER_STOP;
		ev.time = strtoul((*i)[2].c_str(), NULL, 0);

		_evlist.push_back(ev);

		log(LOG_DEBUG, "Initial timer event: server_id=%d, time=%lu, action=%d", ev.server_id, ev.time, ev.action);
	}
}

DEFINE_SUPPORT_MODULE_REGISTRAR(TimerSupportModule);
