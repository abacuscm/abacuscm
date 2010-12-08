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

class ActWhatAmI : public ClientAction {
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *);
};

bool ActWhatAmI::int_process(ClientConnection *cc, MessageBlock *) {
	MessageBlock resp("ok");

	const PermissionSet &perms = cc->permissions();
	size_t pos = 0;
	for (size_t j = 0; j < PERMISSION_COUNT; j++) {
		if (perms[j]) {
			std::ostringstream header;
			header << "permission" << pos;
			resp[header.str()] = getPermissionName(static_cast<Permission>(j));
			pos++;
		}
	}
	return cc->sendMessageBlock(&resp);
}

static ActWhatAmI _act_whatami;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("whatami", PermissionTest::ANY, &_act_whatami);
}
