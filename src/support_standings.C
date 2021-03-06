/**
 * Copyright (c) 2005 - 2006, 2010, 2013 Kroon Infomation Systems,
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
	(void) tm; // not currently used because postdated submissions are not in use

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

	time_t duration = timer->contestDuration();
	if(!duration) {
		db->release();
		pthread_rwlock_unlock(&_lock);
		return false;
	}

	/* First key is team id, second is problem id */
	map<uint32_t, map<uint32_t, vector<SubData> > > problemdata;

	SubmissionList::iterator s;
	for(s = submissions.begin(); s != submissions.end(); ++s) {
		uint32_t sub_id = from_string<uint32_t>((*s)["submission_id"]);

		RunResult state;
		uint32_t utype;
		string comment;

		if(db->getSubmissionState(sub_id, state, utype, comment)) {
			if(state != COMPILE_FAILED && state != JUDGE && state != OTHER) { // we ignore compile failures, and judge deferals.
				SubData tmp;

				uint32_t user_id = db->submission2user_id(sub_id);
				uint32_t group_id = usm->user_group(user_id);
				tmp.correct = state == CORRECT;
				tmp.time = timer->contestTime(group_id, from_string<time_t>((*s)["time"]));

#if 0 // disabled until we can support per-group cached standings
				if (tmp.time > server_time) {
					if (!_postdated_submissions.count(sub_id)) {
						Server::putTimedAction(new TimedStandingsUpdater(tmp.time, user_id, sub_id));
						_postdated_submissions.insert(sub_id);
					}
				} else
#endif
				{
					uint32_t prob_id = from_string<uint32_t>((*s)["prob_id"]);
					tmp.final_only = timer->isBlinded(tmp.time);
					problemdata[user_id][prob_id].push_back(tmp);
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
		int32_t bonus_time;
		teamdata[0].points = 0;
		teamdata[0].time = 0;
		teamdata[0].in_standings = user_perms[PERMISSION_IN_STANDINGS];
		if (usm->user_bonus(t->first, teamdata[0].points, bonus_time))
			teamdata[0].time = -bonus_time;
		teamdata[1] = teamdata[0];
		map<uint32_t, vector<SubData> >::iterator p;
		for(p = t->second.begin(); p != t->second.end(); ++p) {
			int tries = 0;
			bool correct = false;
			time_t correct_time = 0;

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
						teamdata[w].points++;
						teamdata[w].tries[p->first] = tries;
					} else
						teamdata[w].tries[p->first] = -tries;
				}
			}
		}

		/* Check if this is news, in order to update */
		for (int w = 0; w < 2; w++) {
			Standings::iterator pos;
			Standings &standings = w ? _final_standings : _contestant_standings;
			if (!teamdata[w].tries.empty()) {
				pos = standings.find(t->first);
				if (pos == standings.end()
					|| pos->second.points != teamdata[w].points
					|| pos->second.time != teamdata[w].time
					|| pos->second.tries != teamdata[w].tries) {
					standings[t->first] = teamdata[w];
					have_update[w] = true;
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
	mb[cell_name(0, STANDING_RAW_GROUP)] = "Group";
	mb[cell_name(0, STANDING_RAW_CONTESTANT)] = "Contestant";
	mb[cell_name(0, STANDING_RAW_TOTAL_SOLVED)] = "Points";
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
		mb["ncols"] = to_string(ncols);
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

		mb[cell_name(r, STANDING_RAW_ID)] = to_string(i->first);
		mb[cell_name(r, STANDING_RAW_USERNAME)] = usm->username(i->first);
		mb[cell_name(r, STANDING_RAW_FRIENDLYNAME)] = usm->friendlyname(i->first);
		mb[cell_name(r, STANDING_RAW_GROUP)] = usm->groupname(usm->user_group(i->first));
		mb[cell_name(r, STANDING_RAW_CONTESTANT)] = (i->second.in_standings ? "1" : "0");
		mb[cell_name(r, STANDING_RAW_TOTAL_TIME)] = to_string(i->second.time);

		map<uint32_t, int32_t>::const_iterator pc;
		/* Make sure there are no holes by pre-initialising with zeros */
		for(int col = STANDING_RAW_SOLVED; col < ncols; col++)
			mb[cell_name(r, col)] = "0";

		for(pc = i->second.tries.begin(); pc != i->second.tries.end(); ++pc) {
			int col = prob2col[pc->first];
			mb[cell_name(r, col)] = to_string(pc->second);
		}

		mb[cell_name(r, STANDING_RAW_TOTAL_SOLVED)] = to_string(i->second.points);
	}

	mb["nrows"] = to_string(r + 1);

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
	ClientEventRegistry::getInstance().registerEvent("updatestandings", PERMISSION_SEE_STANDINGS);
	updateStandings(0, 0);
}
