/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "clientaction.h"
#include "messageblock.h"
#include "clientconnection.h"
#include "dbcon.h"
#include "logger.h"
#include "standingssupportmodule.h"

#include <vector>
#include <map>
#include <list>
#include <string>
#include <sstream>

using namespace std;

class ActStandings : public ClientAction {
protected:
	virtual bool int_process(ClientConnection*, MessageBlock*mb);
};

bool ActStandings::int_process(ClientConnection*cc, MessageBlock*) {
	StandingsSupportModule *standings = getStandingsSupportModule();
	if (!standings)
		return cc->sendError("Misconfigured Server - unable to calculate standings.");

    uint32_t uType = cc->getProperty("user_type");

	StandingsSupportModule::StandingsPtr s = standings->getStandings(uType);

	if (!s)
		return cc->sendError("Error retrieving standings.");

	MessageBlock mb("ok");

	mb["row_0_0"] = "Team";

	int ncols = 1;

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

	int r = 1;

	StandingsSupportModule::Standings::iterator i;
	for(i = s->begin(); i != s->end(); ++i, ++r) {
		ostringstream headername;
		ostringstream val;
		char time_buffer[64];
		headername << "row_" << r << "_0";
		mb[headername.str()] = db->user_id2name(i->user_id);

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
		row << r;
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
