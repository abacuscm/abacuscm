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
#include <memory>

using namespace std;

class ActStandings : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection*, const MessageBlock*mb);
};

auto_ptr<MessageBlock> ActStandings::int_process(ClientConnection *cc, const MessageBlock *) {
	StandingsSupportModule *standings = getStandingsSupportModule();
	if (!standings)
		return MessageBlock::error("Misconfigured Server - unable to calculate standings.");

	bool final = cc->permissions()[PERMISSION_SEE_FINAL_STANDINGS];
	bool see_all = cc->permissions()[PERMISSION_SEE_ALL_STANDINGS];

	auto_ptr<MessageBlock> mb(MessageBlock::ok());
	if (!standings->getStandings(0, final, see_all, *mb))
		return MessageBlock::error("failed to get standings");

	return mb;
}

static ActStandings _act_standings;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("standings", PERMISSION_AUTH, &_act_standings);
}
