/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __TIMERSUPPORTMODULE_H__
#define __TIMERSUPPORTMODULE_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "supportmodule.h"

#include <pthread.h>
#include <vector>
#include <map>
#include <stdint.h>

#define TIMER_STATUS_STARTED	1
#define TIMER_STATUS_STOPPED	2

#define TIMER_START				TIMER_STATUS_STARTED
#define TIMER_STOP				TIMER_STATUS_STOPPED

class TimerSupportModule : public SupportModule {
	DECLARE_SUPPORT_MODULE(TimerSupportModule);
private:
	struct startstop_event {
		time_t time;
		uint32_t group_id;
		int action;
		struct startstop_event *next;
	};

	TimerSupportModule();
	virtual ~TimerSupportModule();

	struct startstop_event *_evlist;
	pthread_mutex_t _writelock;

	// Maps group to the current contest state for that group. If the group is
	// missing, the contest is assumed to be stopped. This is not internally
	// populated at startup. Instead, act_startstop iterates over groups to
	// populate it using updateGroupState.
	std::map<uint32_t, int> _group_state;
	pthread_mutex_t _group_state_lock;
public:
	virtual void init();

	uint32_t contestDuration();
	uint32_t contestBlindsDuration();
	uint32_t contestTime(uint32_t group_id, time_t real_time = 0);
	uint32_t contestRemaining(uint32_t group_id, time_t real_time = 0) { return contestDuration() - contestTime(group_id, real_time); }
	int contestStatus(uint32_t group_id, time_t real_time = 0);
	/* Returns the real time at which the contest timer will expire. It will return
	 * 0 if it will never expire due to an explicit stop. This includes the case
	 * where an explicit stop occurs as the timer reaches 0. It is intended to be
	 * used to schedule a timed action to tell contestants that the contest is
	 * stopped.
	 */
	time_t contestEndTime(uint32_t group_id);

	// Increasing list of all times at which start/stop events are scheduled
	std::vector<time_t> allStartStopTimes();

	bool scheduleStartStop(uint32_t group_id, time_t time, int action);

	bool isBlinded(uint32_t contest_time);

	// Call this when the start/stop state for a group may be out-of-date.
	// This must be called at server startup (for all groups), after
	// scheduleStartStop for a time in the past, after creating a new group,
	// and after a start/stop event actually occurs.
	// The old and new state are returned.
	virtual void updateGroupState(uint32_t group_id, time_t time, int &old_state, int &new_state);
};

DEFINE_SUPPORT_MODULE_GETTER(TimerSupportModule);

#endif
