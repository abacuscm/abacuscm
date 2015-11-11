/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include <sstream>
#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "logger.h"
#include "dbcon.h"
#include "clienteventregistry.h"
#include "permissionmap.h"
#include "misc.h"

#include <memory>

using namespace std;

class ActWhatAmI : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *);
};

auto_ptr<MessageBlock> ActWhatAmI::int_process(ClientConnection *cc, const MessageBlock *) {
	auto_ptr<MessageBlock> resp(MessageBlock::ok());

	const PermissionSet &perms = cc->permissions();
	size_t pos = 0;
	for (size_t j = 0; j < PERMISSION_COUNT; j++) {
		if (perms[j]) {
			(*resp)["permission" + to_string(pos)] = getPermissionName(static_cast<Permission>(j));
			pos++;
		}
	}
	return resp;
}

static ActWhatAmI _act_whatami;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("whatami", PermissionTest::ANY, &_act_whatami);
}
