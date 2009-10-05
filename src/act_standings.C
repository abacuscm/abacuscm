/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "clientaction.h"
#include "messageblock.h"
#include "clientconnection.h"
#include "dbcon.h"
#include "logger.h"
#include "standingssupportmodule.h"
#include "usersupportmodule.h"

#include <vector>
#include <map>
#include <list>
#include <string>
#include <sstream>

using namespace std;

class ActStandings : public ClientAction {
private:
	typedef map<uint32_t, string> TypesMap;
	TypesMap typenames;
protected:
	virtual bool int_process(ClientConnection*, MessageBlock*mb);
public:
	ActStandings();
};

ActStandings::ActStandings()
{
	typenames[USER_TYPE_ADMIN] = "Admin";
	typenames[USER_TYPE_JUDGE] = "Judge";
	typenames[USER_TYPE_CONTESTANT] = "Contestant";
	typenames[USER_TYPE_NONCONTEST] = "Observer";
}

bool ActStandings::int_process(ClientConnection *cc, MessageBlock *rmb) {
	StandingsSupportModule *standings = getStandingsSupportModule();
	UserSupportModule *usm = getUserSupportModule();
	if (!standings)
		return cc->sendError("Misconfigured Server - unable to calculate standings.");
	if (!usm)
		return cc->sendError("Misconfigured Server - unable to calculate standings.");

    uint32_t uType = cc->getProperty("user_type");
	bool include_non_contest = uType != USER_TYPE_CONTESTANT;

	include_non_contest &= (*rmb)["include_non_contest"] != "no";

	StandingsSupportModule::StandingsPtr s = standings->getStandings(uType);

	if (!s)
		return cc->sendError("Error retrieving standings.");

	MessageBlock mb("ok");

	mb["row_0_0"] = "Team";

	int ncols = 1;

	if (include_non_contest) {
		ncols = 2;
		mb["row_0_1"] = "Type";
	}

	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Error connecting to database.");

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

	int r = 0;

	StandingsSupportModule::Standings::iterator i;
	for(i = s->begin(); i != s->end(); ++i) {
		if (i->user_type != USER_TYPE_CONTESTANT && !include_non_contest)
			continue;
		++r;

		ostringstream headername;
		ostringstream val;
		char time_buffer[64];
		headername << "row_" << r << "_0";
		mb[headername.str()] = usm->username(i->user_id);

		if (include_non_contest) {
			headername.str("");
			headername << "row_" << r << "_1";
			TypesMap::const_iterator tn = typenames.find(i->user_type);
			if (tn != typenames.end())
				mb[headername.str()] = tn->second;
			else
				mb[headername.str()] = "Unknown";
		}

		headername.str("");
		headername << "row_" << r << "_" << (ncols - 1);
		sprintf(time_buffer, "%02d:%02d:%02d",
			i->time / 3600,
			(i->time / 60) % 60,
                        i->time % 60);
		mb[headername.str()] = time_buffer;

		headername.str("");
		headername << "row_" << r << "_" << (ncols - 2);
		val.str("");
		val << i->correct;
		mb[headername.str()] = val.str();

		map<uint32_t, int32_t>::iterator pc;
		for(pc = i->tries.begin(); pc != i->tries.end(); ++pc) {
			int col = prob2col[pc->first];

			headername.str("");
			headername << "row_" << r << "_" << col;

			val.str("");
            if (pc->second > 0)
                val << "(" << pc->second << ")  Yes";
            else
                val << "(" << -pc->second << ")";

			mb[headername.str()] = val.str();
		}
	}

	db->release(); db = NULL;

	{
		ostringstream row;
		row << (r + 1);
		mb["nrows"] = row.str();
	}

	return cc->sendMessageBlock(&mb);
}

static ActStandings _act_standings;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "standings", &_act_standings);
	ClientAction::registerAction(USER_TYPE_JUDGE, "standings", &_act_standings);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "standings", &_act_standings);
}
