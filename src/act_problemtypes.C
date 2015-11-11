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
#include <memory>

using namespace std;

////////////////////////////////////////////////////////////
class ActGetProbTypes : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

auto_ptr<MessageBlock> ActGetProbTypes::int_process(ClientConnection *, const MessageBlock *) {
	vector<string> types = ProblemType::getProblemTypes();

	auto_ptr<MessageBlock> resp(MessageBlock::ok());

	int c = 0;
	for(vector<string>::iterator i = types.begin(); i != types.end(); ++i, ++c) {
		(*resp)["type" + to_string(c)] = *i;
	}

	return resp;
}

////////////////////////////////////////////////////////////
class ActGetProbDescript : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

auto_ptr<MessageBlock> ActGetProbDescript::int_process(ClientConnection *, const MessageBlock *mb) {
	string type = (*mb)["type"];
	if(type == "")
		return MessageBlock::error("You must specify the type to describe");

	string desc = ProblemType::getProblemDescription(type);
	if(desc == "")
		return MessageBlock::error("Error retrieving description for type " + type);

	auto_ptr<MessageBlock> resp(MessageBlock::ok());
	(*resp)["descript"] = desc;

	return resp;
}

////////////////////////////////////////////////////////////
static ActGetProbTypes _act_get_prob_types;
static ActGetProbDescript _act_get_prob_desc;

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("getprobtypes", PERMISSION_PROBLEM_ADMIN, &_act_get_prob_types);
	ClientAction::registerAction("getprobdescript", PERMISSION_PROBLEM_ADMIN, &_act_get_prob_desc);
}
