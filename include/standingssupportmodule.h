/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
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
		uint32_t user_type;
		uint32_t time;
		std::map<uint32_t, int32_t> tries; // sign bit indicates whether correctness has been achieved or not.
	} StandingsData;

	/* Maps user id to that user's standings */
	typedef std::map<uint32_t, StandingsData> Standings;
private:
	/* Each user type has an associated set of flags saying what permissions
	 * they have.
	 */
	enum StandingsFlag {
		STANDINGS_FLAG_SEND = 1,       /* Get standings at all */
		STANDINGS_FLAG_FINAL = 2,      /* See standings in last hour */
		STANDINGS_FLAG_OBSERVER = 4  /* See standings for unofficial entrants */
	};

	pthread_rwlock_t _lock;
	Standings _final_standings;
	Standings _contestant_standings;
	time_t blinds;

	/* Set of submission IDs for which we've created timed actions to add back later. */
	std::set<uint32_t> _postdated_submissions;

	static uint32_t getStandingsFlags(uint32_t user_type);

	/* Callback for TimedStandingsUpdater */
	void timedUpdate(time_t time, uint32_t uid, uint32_t submission_id);

	StandingsSupportModule();
	virtual ~StandingsSupportModule();

	friend class TimedStandingsUpdater;
public:
	/* Indicate that new information might be available for user (or for all
	 * users if uid is zero). If time is non-zero, it should used in place of
	 * time(NULL) in determining which submissions are post-dated (which can
	 * happen when different servers have different start/stop times).
	 */
	bool updateStandings(uint32_t uid, time_t tm);

	/* Takes a copy of the standings and adds it to mb.
	 * user_type is the type of the requesting user
	 * uid is the for whose status should be reported, or 0 to get the full
	 * standings list.
	 */
	bool getStandings(uint32_t user_type, uint32_t uid, MessageBlock &mb);

	virtual void init();
};

DEFINE_SUPPORT_MODULE_GETTER(StandingsSupportModule);

#endif
