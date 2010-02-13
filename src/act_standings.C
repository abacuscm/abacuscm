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

static string cell_name(int row, int col) {
	ostringstream name;
	name << "row_" << row << "_" << col;
	return name.str();
}

bool ActStandings::int_process(ClientConnection *cc, MessageBlock *) {
	StandingsSupportModule *standings = getStandingsSupportModule();
	UserSupportModule *usm = getUserSupportModule();
	if (!standings)
		return cc->sendError("Misconfigured Server - unable to calculate standings.");
	if (!usm)
		return cc->sendError("Misconfigured Server - unable to calculate standings.");

	uint32_t uType = cc->getProperty("user_type");
	bool include_non_contest = uType != USER_TYPE_CONTESTANT;

	StandingsSupportModule::StandingsPtr s = standings->getStandings(uType);

	if (!s)
		return cc->sendError("Error retrieving standings.");

	MessageBlock mb("ok");

	const int COLUMN_ID = 0;
	const int COLUMN_TEAM = 1;
	const int COLUMN_NAME = 2;
	const int COLUMN_CONTESTANT = 3;
	const int COLUMN_TIME = 4;
	int ncols = 5;
	mb[cell_name(0, COLUMN_ID)] = "ID";
	mb[cell_name(0, COLUMN_TEAM)] = "Team";
	mb[cell_name(0, COLUMN_NAME)] = "Name";
	mb[cell_name(0, COLUMN_CONTESTANT)] = "Type";
	mb[cell_name(0, COLUMN_TIME)] = "Time";

	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Error connecting to database.");

	ProblemList probs = db->getProblems();
	map<uint32_t, uint32_t> prob2col;
	ProblemList::iterator pi;

	for(pi = probs.begin(); pi != probs.end(); ++pi) {
		prob2col[*pi] = ncols;
		AttributeList al = db->getProblemAttributes(*pi);
		mb[cell_name(0, ncols)] = al["shortname"];
		ncols++;
	}

	{
		ostringstream ncols_str;
		ncols_str << ncols;
		mb["ncols"] = ncols_str.str();
	}

	int r = 0;

	StandingsSupportModule::Standings::iterator i;
	for(i = s->begin(); i != s->end(); ++i) {
		if (i->user_type != USER_TYPE_CONTESTANT && !include_non_contest)
			continue;
		++r;

		ostringstream val;

		val.str("");
		val << i->user_id;
		mb[cell_name(r, COLUMN_ID)] = val.str();
		mb[cell_name(r, COLUMN_TEAM)] = usm->username(i->user_id);
		mb[cell_name(r, COLUMN_NAME)] = usm->friendlyname(i->user_id);
		mb[cell_name(r, COLUMN_CONTESTANT)] = (i->user_type == USER_TYPE_CONTESTANT ? "1" : "0");

		val.str("");
		val << i->time;
		mb[cell_name(r, COLUMN_TIME)] = val.str();

		map<uint32_t, int32_t>::iterator pc;
		for(pc = i->tries.begin(); pc != i->tries.end(); ++pc) {
			int col = prob2col[pc->first];

			val.str("");
			val << pc->second;
			mb[cell_name(r, col)] = val.str();
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
	ClientAction::registerAction(USER_TYPE_NONCONTEST, "standings", &_act_standings);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "standings", &_act_standings);
}
