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

bool ActStandings::int_process(ClientConnection *cc, MessageBlock *) {
	StandingsSupportModule *standings = getStandingsSupportModule();
	UserSupportModule *usm = getUserSupportModule();
	if (!standings)
		return cc->sendError("Misconfigured Server - unable to calculate standings.");
	if (!usm)
		return cc->sendError("Misconfigured Server - unable to calculate standings.");

	uint32_t uType = cc->getProperty("user_type");

	MessageBlock mb("ok");
	if (!standings->getStandings(uType, 0, mb))
		return cc->sendError("failed to get standings");

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
