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

auto_ptr<MessageBlock> ActContesttime::int_process(ClientConnection*, const MessageBlock*) {
	TimerSupportModule *timer = getTimerSupportModule();
	uint32_t server_id = Server::getId();
	uint32_t contesttime = timer->contestTime(server_id);
	uint32_t contestremain = timer->contestDuration() - contesttime;
	bool running = timer->contestStatus(server_id) == TIMER_STATUS_STARTED;

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
