#include "clientaction.h"
#include "messageblock.h"
#include "clientconnection.h"
#include "dbcon.h"
#include "logger.h"

#include <vector>
#include <map>
#include <list>
#include <string>
#include <sstream>

using namespace std;

typedef struct {
	uint32_t time;
	bool correct;
} SubData;

bool SubDataLessThan(const SubData& s1, const SubData& s2) {
	return s1.time < s2.time;
}

typedef struct {
	uint32_t user_id;
	uint32_t correct;
	uint32_t time;
	map<uint32_t, uint32_t> tries;
} TeamData;

bool TeamDataLessThan(const TeamData& td1, const TeamData& td2) {
	if(td1.correct > td2.correct)
		return true;
	if(td1.correct == td2.correct)
		return td1.time < td2.time;
	return false;
}

class ActStandings : public ClientAction {
protected:
	virtual bool int_process(ClientConnection*, MessageBlock*mb);
};

bool ActStandings::int_process(ClientConnection*cc, MessageBlock*) {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Unable to connect to the database");

	SubmissionList submissions = db->getSubmissions();
	
	map<uint32_t, map<uint32_t, vector<SubData> > > problemdata;
	vector<TeamData> standings;

	SubmissionList::iterator s;
	for(s = submissions.begin(); s != submissions.end(); ++s) {
		uint32_t sub_id = strtoll((*s)["submission_id"].c_str(), NULL, 0);
		
		RunResult state;
		uint32_t utype;
		string comment;
		
		if(db->getSubmissionState(sub_id, state, utype, comment)) {
			if(state != COMPILE_FAILED) { // we ignore compile failures.
				SubData tmp;
				tmp.correct = state == CORRECT;
				tmp.time = db->contestTime(db->submission2server_id(sub_id), strtoll((*s)["time"].c_str(), NULL, 0));

				uint32_t prob_id = strtoll((*s)["prob_id"].c_str(), NULL, 0);
				uint32_t team_id = db->submission2user_id(sub_id);

				problemdata[team_id][prob_id].push_back(tmp);
			}
		}
	}

	map<uint32_t, map<uint32_t, vector<SubData> > >::iterator t;
	for(t = problemdata.begin(); t != problemdata.end(); ++t) {
		TeamData teamdata;
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
			for(s = subs.begin(); s != subs.end() && !correct; ++s) {
				tries++;
				correct = s->correct;
				correct_time = s->time;
			}

			if(correct) {
				teamdata.correct++;
				teamdata.time += (tries - 1) * 20;
				teamdata.time += correct_time;
				teamdata.tries[p->first] = tries;
			}
		}

		standings.push_back(teamdata);
	}

	sort(standings.begin(), standings.end(), TeamDataLessThan);

	MessageBlock mb("ok");

	mb["row_0_0"] = "Contestant";

	int ncols = 1;
	
	ProblemList probs = db->getProblems();
	map<uint32_t, uint32_t> prob2col;
	ProblemList::iterator pi;
	
	for(pi = probs.begin(); pi != probs.end(); ++pi) {
		AttributeList al = db->getProblemAttributes(*pi);
		
		ostringstream col;
		col << ncols;
		prob2col[*pi] = ncols++;
		
		mb["row_0_" + col.str()] = al["shortname"];
	}

	{
		ostringstream col;
		col << ncols++;
		mb["row_0_" + col.str()] = "Solved";

		col.str("");
		col << ncols++;
		mb["row_0_" + col.str()] = "Total time";

		col.str("");
		col << ncols;
		mb["ncols"] = col.str();
	}

	int r = 1;
	
	vector<TeamData>::iterator i;
	for(i = standings.begin(); i != standings.end(); ++i, ++r) {
		ostringstream headername;
		ostringstream val;
		headername << "row_" << r << "_0";
		mb[headername.str()] = db->user_id2name(i->user_id);

		headername.str("");
		headername << "row_" << r << "_" << (ncols - 1);
		val << i->time;
		mb[headername.str()] = val.str();

		headername.str("");
		headername << "row_" << r << "_" << (ncols - 2);
		val.str("");
		val << i->correct;
		mb[headername.str()] = val.str();

		map<uint32_t, uint32_t>::iterator pc;
		for(pc = i->tries.begin(); pc != i->tries.end(); ++pc) {
			int col = prob2col[pc->first];

			headername.str("");
			headername << "row_" << r << "_" << col;

			val.str("");
			val << pc->second;

			mb[headername.str()] = val.str();
		}
	}

	{
		ostringstream row;
		row << r;
		mb["nrows"] = row.str();
	}

	db->release();db=NULL;
	
	return cc->sendMessageBlock(&mb);
}

static ActStandings _act_standings;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "standings", &_act_standings);
	ClientAction::registerAction(USER_TYPE_JUDGE, "standings", &_act_standings);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "standings", &_act_standings);
}
