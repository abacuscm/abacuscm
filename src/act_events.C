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

#include <memory>

using namespace std;

class Act_ClientEventRegistry : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

auto_ptr<MessageBlock> Act_ClientEventRegistry::int_process(ClientConnection* cc, const MessageBlock* mb) {
	ClientEventRegistry& evReg = ClientEventRegistry::getInstance();

	string action = (*mb)["action"];
	string event = (*mb)["event"];

	if(action == "subscribe") {
		if(evReg.registerClient(event, cc))
			return MessageBlock::ok();
		else
			return MessageBlock::error("No such event or permission denied");
	} else if(action == "unsubscribe") {
		if (evReg.deregisterClient(event, cc))
			return MessageBlock::ok();
		else
			return MessageBlock::error("No such event or permission denied");
	} else
		return MessageBlock::error("Unknown action");
}

static Act_ClientEventRegistry _act_eventreg;

extern "C" void abacuscm_mod_init() {
	// No permissions required because each event specifies who may register
	ClientAction::registerAction("eventmap", PermissionTest::ANY, &_act_eventreg);
}
