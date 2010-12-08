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
protected:
	virtual bool int_process(ClientConnection*, MessageBlock*mb);
};

bool ActStandings::int_process(ClientConnection *cc, MessageBlock *) {
	StandingsSupportModule *standings = getStandingsSupportModule();
	if (!standings)
		return cc->sendError("Misconfigured Server - unable to calculate standings.");

	bool final = cc->permissions()[PERMISSION_SEE_FINAL_STANDINGS];
	bool see_all = cc->permissions()[PERMISSION_SEE_ALL_STANDINGS];

	MessageBlock mb("ok");
	if (!standings->getStandings(0, final, see_all, mb))
		return cc->sendError("failed to get standings");

	return cc->sendMessageBlock(&mb);
}

static ActStandings _act_standings;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("standings", PERMISSION_AUTH, &_act_standings);
}
