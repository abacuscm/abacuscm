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

class ActRunning : public ClientAction {
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

bool ActRunning::int_process(ClientConnection *cc, MessageBlock*) {
	MessageBlock mb("ok");
	if(getTimerSupportModule()->contestStatus(Server::getId()) == TIMER_STATUS_STARTED)
		mb["status"] = "running";
	else
		mb["status"] = "stopped";

	return cc->sendMessageBlock(&mb);
}

static ActRunning _act_running;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "conteststatus", &_act_running);
	ClientAction::registerAction(USER_TYPE_JUDGE, "conteststatus", &_act_running);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "conteststatus", &_act_running);
}
