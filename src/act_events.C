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
#include "logger.h"
#include "messageblock.h"
#include "clientconnection.h"
#include "clienteventregistry.h"
#include "misc.h"

class Act_ClientEventRegistry : public ClientAction {
protected:
	virtual void int_process(ClientConnection *cc, MessageBlock *mb);
};

void Act_ClientEventRegistry::int_process(ClientConnection* cc, MessageBlock* mb) {
	ClientEventRegistry& evReg = ClientEventRegistry::getInstance();

	std::string action = (*mb)["action"];
	std::string event = (*mb)["event"];

	if(action == "subscribe") {
		if(evReg.registerClient(event, cc))
			return cc->reportSuccess();
		else
			return cc->sendError("No such event or permission denied");
	} else if(action == "unsubscribe") {
		if (evReg.deregisterClient(event, cc))
			return cc->reportSuccess();
		else
			return cc->sendError("No such event or permission denied");
	} else
		return cc->sendError("Unknown action");
}

static Act_ClientEventRegistry _act_eventreg;

extern "C" void abacuscm_mod_init() {
	// No permissions required because each event specifies who may register
	ClientAction::registerAction("eventmap", PermissionTest::ANY, &_act_eventreg);
}
