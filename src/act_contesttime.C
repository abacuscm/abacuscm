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
#include "clientconnection.h"
#include "messageblock.h"
#include "server.h"
#include "timersupportmodule.h"
#include "misc.h"

#include <sstream>
#include <memory>

using namespace std;

class ActContesttime : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection* cc, const MessageBlock*mb);
};

auto_ptr<MessageBlock> ActContesttime::int_process(ClientConnection*cc, const MessageBlock*) {
	TimerSupportModule *timer = getTimerSupportModule();
	time_t contesttime = timer->contestTime(cc->getGroupId());
	time_t contestremain = timer->contestDuration() - contesttime;
	bool running = timer->contestStatus(cc->getGroupId()) == TIMER_STATUS_STARTED;

	ostringstream os;

	auto_ptr<MessageBlock> res(MessageBlock::ok());
	(*res)["running"] = running ? "yes" : "no";

	os << contesttime;
	(*res)["time"] = os.str();

	os.str("");
	os << contestremain;
	(*res)["remain"] = os.str();

	return res;
}

static ActContesttime _act_contesttime;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("contesttime", PERMISSION_AUTH, &_act_contesttime);
}
