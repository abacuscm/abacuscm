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
	virtual void int_process(ClientConnection *cc, MessageBlock *mb);
};

void ActRunning::int_process(ClientConnection *cc, MessageBlock*) {
	MessageBlock mb("ok");
	if(getTimerSupportModule()->contestStatus(Server::getId()) == TIMER_STATUS_STARTED)
		mb["status"] = "running";
	else
		mb["status"] = "stopped";

	cc->sendMessageBlock(&mb);
}

static ActRunning _act_running;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("conteststatus", PERMISSION_AUTH, &_act_running);
}
