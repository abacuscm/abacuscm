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

#include <memory>

using namespace std;

class ActRunning : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

auto_ptr<MessageBlock> ActRunning::int_process(ClientConnection *cc, const MessageBlock*) {
	auto_ptr<MessageBlock> mb(MessageBlock::ok());
	if(getTimerSupportModule()->contestStatus(cc->getGroupId()) == TIMER_STATUS_STARTED)
		(*mb)["status"] = "running";
	else
		(*mb)["status"] = "stopped";

	return mb;
}

static ActRunning _act_running;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("conteststatus", PERMISSION_AUTH, &_act_running);
}
