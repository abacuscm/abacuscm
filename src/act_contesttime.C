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

#include <sstream>

class ActContesttime : public ClientAction {
protected:
	virtual bool int_process(ClientConnection* cc, MessageBlock*mb);
};

bool ActContesttime::int_process(ClientConnection* cc, MessageBlock*) {
	TimerSupportModule *timer = getTimerSupportModule();
	uint32_t server_id = Server::getId();
	uint32_t contesttime = timer->contestTime(server_id);
	uint32_t contestremain = timer->contestDuration() - contesttime;
	bool running = timer->contestStatus(server_id);

	std::ostringstream os;

	MessageBlock res("ok");
	res["running"] = running ? "yes" : "no";

	os << contesttime;
	res["time"] = os.str();

	os.str("");
	os << contestremain;
	res["remain"] = os.str();

	return cc->sendMessageBlock(&res);
}

static ActContesttime _act_contesttime;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "contesttime", &_act_contesttime);
	ClientAction::registerAction(USER_TYPE_JUDGE, "contesttime", &_act_contesttime);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "contesttime", &_act_contesttime);
}
