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
#include "logger.h"
#include "dbcon.h"
#include "clienteventregistry.h"

class ActWhatAmI : public ClientAction {
private:
	std::map<uint32_t, std::string> _typemap;
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *);
public:
	ActWhatAmI();
};

ActWhatAmI::ActWhatAmI() {
	_typemap[USER_TYPE_NONE] = "none";
	_typemap[USER_TYPE_ADMIN] = "admin";
	_typemap[USER_TYPE_JUDGE] = "judge";
	_typemap[USER_TYPE_CONTESTANT] = "contestant";
	_typemap[USER_TYPE_OBSERVER] = "observer";
}

bool ActWhatAmI::int_process(ClientConnection *cc, MessageBlock *) {
	uint32_t user_type = cc->getProperty("user_type");

	MessageBlock resp("ok");
	std::map<uint32_t, std::string>::const_iterator i = _typemap.find(user_type);
	if (i != _typemap.end())
		resp["type"] = i->second;
	else
		resp["type"] = "none";
	return cc->sendMessageBlock(&resp);
}

static ActWhatAmI _act_whatami;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction(USER_TYPE_NONE, "whatami", &_act_whatami);
	ClientAction::registerAction(USER_TYPE_ADMIN, "whatami", &_act_whatami);
	ClientAction::registerAction(USER_TYPE_JUDGE, "whatami", &_act_whatami);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "whatami", &_act_whatami);
	ClientAction::registerAction(USER_TYPE_OBSERVER, "whatami", &_act_whatami);
}
