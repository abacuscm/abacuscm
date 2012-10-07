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

	void dumpevlist();
public:
	virtual void init();

	uint32_t contestDuration();
	uint32_t contestTime(uint32_t group_id, time_t real_time = 0);
	uint32_t contestRemaining(uint32_t group_id, time_t real_time = 0) { return contestDuration() - contestTime(group_id, real_time); }

	bool nextScheduledStartStopAfter(uint32_t group_id, time_t after_time, time_t *next_time, int *next_action);
	bool scheduleStartStop(uint32_t group_id, time_t time, int action);

	int contestStatus(uint32_t group_id, time_t real_time = 0);
};

DEFINE_SUPPORT_MODULE_GETTER(TimerSupportModule);

#endif
