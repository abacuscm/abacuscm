/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "standingssupportmodule.h"
#include "scoped_lock.h"
#include "usersupportmodule.h"
#include "timersupportmodule.h"
#include "logger.h"
#include "dbcon.h"
#include "eventregister.h"
#include "messageblock.h"
#include "timedaction.h"
#include "server.h"

#include <string>
#include <time.h>

using namespace std;

class TimedStandingsUpdater : public TimedAction
{
private:
	TimedStandingsUpdater *_active;
public:
	TimedStandingsUpdater(time_t _time) : TimedAction(_time) { _active = this; };

	void perform();
};

void TimedStandingsUpdater::perform() {
	if(this == _active)
		getStandingsSupportModule()->timedUpdate();
}

StandingsSupportModule::StandingsSupportModule()
{
	dirty = true;
	pthread_mutex_init(&_update_lock, NULL);
}

StandingsSupportModule::~StandingsSupportModule()
{
}

typedef struct
{
	time_t time;
	bool correct;
	bool final_only;
} SubData;

bool SubDataLessThan(const SubData& s1, const SubData& s2)
{
	return s1.time < s2.time;
}

bool StandingsDataLessThan(const StandingsSupportModule::StandingsData& td1, const StandingsSupportModule::StandingsData& td2)
{
	if(td1.correct > td2.correct)
		return true;
	if(td1.correct == td2.correct)
		return td1.time < td2.time;
	return false;
}

bool StandingsSupportModule::updateStandings()
{
	ScopedLock lock_update(&_update_lock);

	if(!dirty)
		return true;

	TimerSupportModule *timer = getTimerSupportModule();

	if(!timer) {
		log(LOG_CRIT, "Error obtaining TimerSupportModule.");
		return false;
	}

	DbCon *db = DbCon::getInstance();
	if(!db) {
		log(LOG_CRIT, "Error obtaining DB Support.");
		return false;
	}

	SubmissionList submissions = db->getSubmissions();

	uint32_t duration = timer->contestDuration();
	if(!duration) {
		db->release();
		return false;
	}

    map<uint32_t, map<uint32_t, vector<SubData> > > problemdata;

	time_t future_time = 0;
	time_t server_time = timer->contestTime(Server::getId());

	SubmissionList::iterator s;
	for(s = submissions.begin(); s != submissions.end(); ++s) {
		uint32_t sub_id = strtoll((*s)["submission_id"].c_str(), NULL, 0);

		RunResult state;
		uint32_t utype;
		string comment;

		if(db->getSubmissionState(sub_id, state, utype, comment)) {
			if(state != COMPILE_FAILED) { // we ignore compile failures.
				SubData tmp;

				uint32_t server_id = db->submission2server_id(sub_id);
                tmp.correct = state == CORRECT;
				tmp.time = timer->contestTime(server_id, strtoull((*s)["time"].c_str(), NULL, 0));

				if (tmp.time > server_time) {
					if (!future_time || tmp.time - server_time < future_time)
						future_time = tmp.time - server_time;
				} else {
					uint32_t timeRemaining = duration - timer->contestTime(server_id);
					uint32_t tRemain = duration - tmp.time;

					uint32_t prob_id = strtoll((*s)["prob_id"].c_str(), NULL, 0);
					uint32_t team_id = db->submission2user_id(sub_id);

					tmp.final_only = (timeRemaining < 3600) && (tRemain < 3600);

					problemdata[team_id][prob_id].push_back(tmp);
				}
			}
		}
	}

	db->release(); db = NULL;

	if (future_time)
		Server::putTimedAction(new TimedStandingsUpdater(server_time + future_time));

	for (int w = 0; w < 2; ++w) {
		StandingsPtr standings(new Standings);
		map<uint32_t, map<uint32_t, vector<SubData> > >::iterator t;
		for(t = problemdata.begin(); t != problemdata.end(); ++t) {
			StandingsData teamdata;
			teamdata.correct = 0;
			teamdata.time = 0;
			teamdata.user_id = t->first;

			map<uint32_t, vector<SubData> >::iterator p;
			for(p = t->second.begin(); p != t->second.end(); ++p) {
				int tries = 0;
				bool correct = false;
				uint32_t correct_time;

				vector<SubData> &subs = p->second;
				sort(subs.begin(), subs.end(), SubDataLessThan);

				vector<SubData>::iterator s;
				for(s = subs.begin(); s != subs.end() && !correct && (w || !s->final_only); ++s) {
					tries++;
					correct = s->correct;
					correct_time = s->time;
				}

				if(correct) {
					teamdata.correct++;
					teamdata.time += (tries - 1) * 20 * 60;
					teamdata.time += correct_time;
					teamdata.tries[p->first] = tries;
				} else
					teamdata.tries[p->first] = -tries;
			}

			standings->push_back(teamdata);
		}

		sort(standings->begin(), standings->end(), StandingsDataLessThan);

		if(w)
			_final_standings = standings;
		else
			_contestant_standings = standings;
	}

	dirty = false;

	return true;
}

bool StandingsSupportModule::checkStandings()
{
	return dirty ? updateStandings() : true;
}

void StandingsSupportModule::notifySolution(time_t /* submit_time */)
{
	{
		ScopedLock lock_update(&_update_lock);
		dirty = true;
	}

	MessageBlock st("standings");
	EventRegister::getInstance().broadcastEvent(&st);
}

void StandingsSupportModule::timedUpdate()
{
	notifySolution(time(NULL));
}

const StandingsSupportModule::StandingsPtr StandingsSupportModule::getStandings(uint32_t user_type)
{
	if (checkStandings()) {
		if(user_type == USER_TYPE_CONTESTANT)
			return _contestant_standings;
		else
			return _final_standings;
	}

	return StandingsPtr();
}

DEFINE_SUPPORT_MODULE_REGISTRAR(StandingsSupportModule);
