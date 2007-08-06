/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "clientaction.h"
#include "logger.h"
#include "messageblock.h"
#include "clientconnection.h"
#include "eventregister.h"

class Act_EventRegister : public ClientAction {
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

bool Act_EventRegister::int_process(ClientConnection* cc, MessageBlock* mb) {
	EventRegister& evReg = EventRegister::getInstance();

	std::string action = (*mb)["action"];
	std::string event = (*mb)["event"];

	if(action == "subscribe") {
		if(evReg.registerClient(event, cc))
			return cc->reportSuccess();
		else
			return cc->sendError("No such event");
	} else if(action == "unsubscribe") {
		evReg.deregisterClient(event, cc);
		return cc->reportSuccess();
	} else
		return cc->sendError("Unknown action");
}

static Act_EventRegister _act_eventreg;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "eventmap", &_act_eventreg);
	ClientAction::registerAction(USER_TYPE_JUDGE, "eventmap", &_act_eventreg);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "eventmap", &_act_eventreg);
}
