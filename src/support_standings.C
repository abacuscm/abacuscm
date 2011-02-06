/**
 * Copyright (c) 2005 - 2006, 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "acmconfig.h"
#include "standingssupportmodule.h"
#include "scoped_lock.h"
#include "usersupportmodule.h"
#include "timersupportmodule.h"
#include "logger.h"
#include "dbcon.h"
#include "clienteventregistry.h"
#include "messageblock.h"
#include "timedaction.h"
#include "server.h"
#include "misc.h"
#include "permissionmap.h"

#include <string>
#include <cassert>
#include <algorithm>
#include <sstream>
#include <time.h>

using namespace std;

DEFINE_SUPPORT_MODULE(StandingsSupportModule);

class TimedStandingsUpdater : public TimedAction
{
private:
	uint32_t _user_id;
	uint32_t _submission_id;
public:
	TimedStandingsUpdater(time_t _time, uint32_t user_id, uint32_t submission_id)
		: TimedAction(_time), _user_id(user_id), _submission_id(submission_id) {}

	void perform();
};

void TimedStandingsUpdater::perform() {
	getStandingsSupportModule()->timedUpdate(processingTime(), _user_id, _submission_id);
}

StandingsSupportModule::StandingsSupportModule()
{
	blinds = 0;
	pthread_rwlock_init(&_lock, NULL);
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
	if (s1.final_only != s2.final_only)
		return s2.final_only;
	return s1.time < s2.time;
}

bool StandingsSupportModule::updateStandings(uint32_t uid, time_t tm)
{
	pthread_rwlock_wrlock(&_lock);

	TimerSupportModule *timer = getTimerSupportModule();
	UserSupportModule *usm = getUserSupportModule();

	if(!timer) {
		log(LOG_CRIT, "Error obtaining TimerSupportModule.");
		pthread_rwlock_unlock(&_lock);
		return false;
	}

	if (!usm) {
		log(LOG_CRIT, "Error obtaining UserSupportModule.");
		pthread_rwlock_unlock(&_lock);
		return false;
	}

	DbCon *db = DbCon::getInstance();
	if(!db) {
		log(LOG_CRIT, "Error obtaining DB Support.");
		pthread_rwlock_unlock(&_lock);
		return false;
	}

	SubmissionList submissions = db->getSubmissions(uid);

	uint32_t duration = timer->contestDuration();
	if(!duration) {
		db->release();
		pthread_rwlock_unlock(&_lock);
		return false;
	}

	/* First key is team id, second is problem id */
	map<uint32_t, map<uint32_t, vector<SubData> > > problemdata;

	time_t server_time = timer->contestTime(Server::getId(), tm);

	SubmissionList::iterator s;
	for(s = submissions.begin(); s != submissions.end(); ++s) {
		uint32_t sub_id = strtoll((*s)["submission_id"].c_str(), NULL, 0);

		RunResult state;
		uint32_t utype;
		string comment;

		if(db->getSubmissionState(sub_id, state, utype, comment)) {
			if(state != COMPILE_FAILED && state != JUDGE && state != OTHER) { // we ignore compile failures, and judge deferals.
				SubData tmp;

				uint32_t team_id = db->submission2user_id(sub_id);
				uint32_t server_id = db->submission2server_id(sub_id);
				tmp.correct = state == CORRECT;
				tmp.time = timer->contestTime(server_id, strtoull((*s)["time"].c_str(), NULL, 0));

				if (tmp.time > server_time) {
					if (!_postdated_submissions.count(sub_id)) {
						Server::putTimedAction(new TimedStandingsUpdater(tmp.time, team_id, sub_id));
						_postdated_submissions.insert(sub_id);
					}
				} else {
					uint32_t timeRemaining = duration - timer->contestTime(server_id, tm);
					uint32_t tRemain = duration - tmp.time;

					uint32_t prob_id = strtoll((*s)["prob_id"].c_str(), NULL, 0);
					uint32_t team_id = db->submission2user_id(sub_id);

					// TODO: This looks like a bug.  Suspect the check should be on tRemain only.
					tmp.final_only = (timeRemaining < blinds) && (tRemain < blinds);

					problemdata[team_id][prob_id].push_back(tmp);
				}
			}
		}
	}

	db->release(); db = NULL;

	map<uint32_t, map<uint32_t, vector<SubData> > >::iterator t;
	bool have_update[2] = {false, false};

	for(t = problemdata.begin(); t != problemdata.end(); ++t) {
		// 0/1 indices are for contestants/final standings
		StandingsData teamdata[2];
		UserType user_type = static_cast<UserType>(usm->usertype(t->first));
		const PermissionSet &user_perms = PermissionMap::getInstance()->getPermissions(user_type);
		teamdata[0].time = 0;
		teamdata[0].in_standings = user_perms[PERMISSION_IN_STANDINGS];
		teamdata[1].time = 0;
		teamdata[1].in_standings = teamdata[0].in_standings;
		map<uint32_t, vector<SubData> >::iterator p;
		for(p = t->second.begin(); p != t->second.end(); ++p) {
			int tries = 0;
			bool correct = false;
			uint32_t correct_time = 0;

			vector<SubData> &subs = p->second;
			stable_sort(subs.begin(), subs.end(), SubDataLessThan);

			vector<SubData>::iterator s;
			s = subs.begin();
			for (int w = 0; w < 2; ++w) {
				while (s != subs.end() && !correct && (w || !s->final_only)) {
					tries++;
					correct = s->correct;
					correct_time = s->time;
					++s;
				}

				if (tries) {
					if(correct) {
						teamdata[w].time += (tries - 1) * 20 * 60;
						teamdata[w].time += correct_time;
						teamdata[w].tries[p->first] = tries;
					} else
						teamdata[w].tries[p->first] = -tries;
				}

				/* Check if this is news, in order to update */
				Standings::iterator pos;
				Standings &standings = w ? _final_standings : _contestant_standings;
				if (!teamdata[w].tries.empty()) {
					pos = standings.find(t->first);
					if (pos == standings.end()
						|| pos->second.time != teamdata[w].time
						|| pos->second.tries != teamdata[w].tries) {
						standings[t->first] = teamdata[w];
						have_update[w] = true;
					}
				}
			}
		}
	}

	for (int w = 0; w < 2; w++)
		if (have_update[w]) {
			for (int see_all = 0; see_all < 2; see_all++) {
				PermissionTest pt = PERMISSION_SEE_STANDINGS;
				if (w)
					pt = pt && PERMISSION_SEE_FINAL_STANDINGS;
				else
					pt = pt && !PERMISSION_SEE_FINAL_STANDINGS;

				if (see_all)
					pt = pt && PERMISSION_SEE_ALL_STANDINGS;
				else
					pt = pt && !PERMISSION_SEE_ALL_STANDINGS;

				MessageBlock mb("updatestandings");
				if (getStandingsInternal(uid, w, see_all, mb, true)
					&& mb["nrows"] != "0")
					ClientEventRegistry::getInstance().broadcastEvent(0, pt, &mb);
			}
		}

	/* Note: this lock must be held until after the update broadcast, because
	 * clients assume that standings updates are monotonicly providing newer
	 * information. If the lock is not held, then concurrent calls to
	 * updateStandings can return results out-of-order as they race to enqueue
	 * the broadcast messages.
	 *
	 * Because broadcastEvent simply enqueues the broadcast to other threads,
	 * it should not block for any significant amount of time.
	 */
	pthread_rwlock_unlock(&_lock);

	return true;
}

static string cell_name(int row, int col) {
	ostringstream name;
	name << "row_" << row << "_" << col;
	return name.str();
}

bool StandingsSupportModule::getStandingsInternal(uint32_t uid, bool final, bool see_all, MessageBlock &mb, bool caller_locks)
{
	UserSupportModule *usm = getUserSupportModule();
	if (!usm) {
		log(LOG_CRIT, "Error obtaining UserSupportModule.");
		return false;
	}

	DbCon *db = DbCon::getInstance();
	if(!db)
		return false;

	int ncols = STANDING_RAW_SOLVED;
	mb[cell_name(0, STANDING_RAW_ID)] = "ID";
	mb[cell_name(0, STANDING_RAW_USERNAME)] = "Team";
	mb[cell_name(0, STANDING_RAW_FRIENDLYNAME)] = "Name";
	mb[cell_name(0, STANDING_RAW_CONTESTANT)] = "Contestant";
	mb[cell_name(0, STANDING_RAW_TOTAL_SOLVED)] = "Solved";
	mb[cell_name(0, STANDING_RAW_TOTAL_TIME)] = "Time";

	ProblemList probs = db->getProblems();
	map<uint32_t, uint32_t> prob2col;
	ProblemList::iterator pi;

	for(pi = probs.begin(); pi != probs.end(); ++pi) {
		prob2col[*pi] = ncols;
		AttributeList al = db->getProblemAttributes(*pi);
		mb[cell_name(0, ncols)] = al["shortname"];
		ncols++;
	}

	db->release(); db = NULL;

	{
		ostringstream ncols_str;
		ncols_str << ncols;
		mb["ncols"] = ncols_str.str();
	}

	if (!caller_locks)
		pthread_rwlock_rdlock(&_lock);
	const Standings &standings = 
		final ? _final_standings : _contestant_standings;

	int r = 0;

	Standings::const_iterator first, last, i;
	if (uid != 0) {
		first = standings.lower_bound(uid);
		last = standings.upper_bound(uid);
	}
	else {
		first = standings.begin();
		last = standings.end();
	}
	for(i = first; i != last; ++i) {
		if (!see_all && !i->second.in_standings)
			continue;
		++r;

		ostringstream val;

		val.str("");
		val << i->first;
		mb[cell_name(r, STANDING_RAW_ID)] = val.str();
		mb[cell_name(r, STANDING_RAW_USERNAME)] = usm->username(i->first);
		mb[cell_name(r, STANDING_RAW_FRIENDLYNAME)] = usm->friendlyname(i->first);
		mb[cell_name(r, STANDING_RAW_CONTESTANT)] = (i->second.in_standings ? "1" : "0");

		val.str("");
		val << i->second.time;
		mb[cell_name(r, STANDING_RAW_TOTAL_TIME)] = val.str();

		map<uint32_t, int32_t>::const_iterator pc;
		/* Make sure there are no holes by pre-initialising with zeros */
		for(int col = STANDING_RAW_SOLVED; col < ncols; col++)
			mb[cell_name(r, col)] = "0";

		int solved = 0;
		for(pc = i->second.tries.begin(); pc != i->second.tries.end(); ++pc) {
			int col = prob2col[pc->first];

			val.str("");
			val << pc->second;
			mb[cell_name(r, col)] = val.str();
			if (pc->second > 0)
				solved++;
		}

		val.str("");
		val << solved;
		mb[cell_name(r, STANDING_RAW_TOTAL_SOLVED)] = val.str();
	}

	{
		ostringstream row;
		row << (r + 1);
		mb["nrows"] = row.str();
	}

	if (!caller_locks)
		pthread_rwlock_unlock(&_lock);
	return true;
}

bool StandingsSupportModule::getStandings(uint32_t uid, bool final, bool see_all, MessageBlock &mb) {
	return getStandingsInternal(uid, final, see_all, mb, false);
}

void StandingsSupportModule::timedUpdate(time_t time, uint32_t uid, uint32_t submission_id) {
	updateStandings(uid, time);
	_postdated_submissions.erase(submission_id);
}

void StandingsSupportModule::init() {
	Config &conf = Config::getConfig();

	TimerSupportModule *timer = getTimerSupportModule();

	uint32_t duration;
	if(!timer) {
		log(LOG_CRIT, "Error obtaining TimerSupportModule.");
		duration = 5 * 60 * 60;
	}

	duration = timer->contestDuration();
	blinds = strtoul(conf["contest"]["blinds"].c_str(), NULL, 0);
	if (blinds == 0) {
		log(LOG_WARNING, "blinds is NOT set explicitly, defaulting to 1 hour for backwards compatibility.");
		blinds = 3600;
	}
	if (blinds > duration / 2)
		log(LOG_WARNING, "Blinds is longer than half the contest - this is most likely wrong.");

	ClientEventRegistry::getInstance().registerEvent("updatestandings", PERMISSION_SEE_STANDINGS);
	updateStandings(0, 0);
}
