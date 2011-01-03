/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "problemtype.h"
#include "clientaction.h"
#include "clientconnection.h"
#include "logger.h"
#include "messageblock.h"
#include "misc.h"

#include <sstream>

using namespace std;

////////////////////////////////////////////////////////////
class ActGetProbTypes : public ClientAction {
protected:
	virtual void int_process(ClientConnection *cc, MessageBlock *mb);
};

void ActGetProbTypes::int_process(ClientConnection *cc, MessageBlock *) {
	vector<string> types = ProblemType::getProblemTypes();

	MessageBlock resp("ok");

	int c = 0;
	for(vector<string>::iterator i = types.begin(); i != types.end(); ++i, ++c) {
		ostringstream t;
		t << "type" << c;
		resp[t.str()] = *i;
	}

	cc->sendMessageBlock(&resp);
}

////////////////////////////////////////////////////////////
class ActGetProbDescript : public ClientAction {
protected:
	virtual void int_process(ClientConnection *cc, MessageBlock *mb);
};

void ActGetProbDescript::int_process(ClientConnection *cc, MessageBlock *mb) {
	string type = (*mb)["type"];
	if(type == "")
		return cc->sendError("You must specify the type to describe");

	string desc = ProblemType::getProblemDescription(type);
	if(desc == "")
		return cc->sendError("Error retrieving description for type " + type);

	MessageBlock resp("ok");
	resp["descript"] = desc;

	cc->sendMessageBlock(&resp);
}

////////////////////////////////////////////////////////////
static ActGetProbTypes _act_get_prob_types;
static ActGetProbDescript _act_get_prob_desc;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("getprobtypes", PERMISSION_PROBLEM_ADMIN, &_act_get_prob_types);
	ClientAction::registerAction("getprobdescript", PERMISSION_PROBLEM_ADMIN, &_act_get_prob_desc);
}
