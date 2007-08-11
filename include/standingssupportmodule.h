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
#include <boost/shared_ptr.hpp>
#include <map>
#include <vector>

class StandingsSupportModule : public SupportModule {
	DECLARE_SUPPORT_MODULE(StandingsSupportModule);
public:
	typedef struct
	{
		uint32_t user_id;
		uint32_t correct;
		uint32_t time;
		std::map<uint32_t, int32_t> tries; // sign bit indicates whether correctness has been achieved or not.
	} StandingsData;

	typedef std::vector<StandingsData> Standings;

	typedef boost::shared_ptr<Standings> StandingsPtr;
private:
	pthread_mutex_t _update_lock;
	StandingsPtr _final_standings;
	StandingsPtr _contestant_standings;
	bool dirty;

	StandingsSupportModule();
	virtual ~StandingsSupportModule();

	bool updateStandings();

	bool checkStandings();

public:
	const StandingsPtr getStandings(uint32_t user_type);
	void notifySolution(time_t submit_time);
	void timedUpdate();
};

DEFINE_SUPPORT_MODULE_GETTER(StandingsSupportModule);

#endif
