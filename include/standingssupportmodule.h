/**
 * Copyright (c) 2005 - 2006, 2013 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * This file implements the standings algorithm for the ACM contest.
 *
 * TODO: This needs to become an abstraction so that different types of
 * contests can have different standing calculation models.
 * $Id$
 */
#ifndef __STANDINGSSUPPORTMODULE_H__
#define __STANDINGSSUPPORTMODULE_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "supportmodule.h"

#include <stdint.h>
#include <pthread.h>
#include <map>
#include <set>
#include <vector>

class TimedStandingsUpdater;
class MessageBlock;

class StandingsSupportModule : public SupportModule {
	DECLARE_SUPPORT_MODULE(StandingsSupportModule);
public:
	typedef struct
	{
		bool in_standings;                 // user has PERMISSION_IN_STANDINGS
		time_t time;
		int points;
		std::map<uint32_t, int32_t> tries; // sign bit indicates whether correctness has been achieved or not.
	} StandingsData;

	/* Maps user id to that user's standings */
	typedef std::map<uint32_t, StandingsData> Standings;
private:
	pthread_rwlock_t _lock;
	Standings _final_standings;
	Standings _contestant_standings;
	time_t blinds;

	/* Set of submission IDs for which we've created timed actions to add back later. */
	std::set<uint32_t> _postdated_submissions;

	/* Callback for TimedStandingsUpdater */
	void timedUpdate(time_t time, uint32_t uid, uint32_t submission_id);

	StandingsSupportModule();
	virtual ~StandingsSupportModule();

	/* Equivalent to getStandings, but optionally allows the caller to
	 * handle locking.
	 */
	bool getStandingsInternal(uint32_t uid, bool final, bool see_all, MessageBlock &mb, bool caller_locks);

	friend class TimedStandingsUpdater;
public:
	/* Indicate that new information might be available for user (or for all
	 * users if uid is zero). If time is non-zero, it should used in place of
	 * time(NULL) in determining which submissions are post-dated (which can
	 * happen when different servers have different start/stop times).
	 */
	bool updateStandings(uint32_t uid, time_t tm);

	/* Takes a copy of the standings and adds it to mb.
	 * uid is the user for whom status should be reported, or 0 to get the full
	 * standings list.
	 * If final is true, include last-hour results.
	 * If see_all is false, show only users with PERMISSION_IN_STANDINGS.
	 */
	bool getStandings(uint32_t uid, bool final, bool see_all, MessageBlock &mb);

	virtual void init();
};

DEFINE_SUPPORT_MODULE_GETTER(StandingsSupportModule);

#endif
