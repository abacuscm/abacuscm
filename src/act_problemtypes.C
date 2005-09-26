#include "problemtype.h"
#include "clientaction.h"
#include "clientconnection.h"
#include "logger.h"
#include "messageblock.h"

#include <sstream>

using namespace std;

////////////////////////////////////////////////////////////
class ActGetProbTypes : public ClientAction {
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

bool ActGetProbTypes::int_process(ClientConnection *cc, MessageBlock *) {
	vector<string> types = ProblemType::getProblemTypes();

	MessageBlock resp("ok");

	int c = 0;
	for(vector<string>::iterator i = types.begin(); i != types.end(); ++i, ++c) {
		ostringstream t;
		t << "type" << c;
		resp[t.str()] = *i;
	}

	return cc->sendMessageBlock(&resp);
}

////////////////////////////////////////////////////////////
class ActGetProbDescript : public ClientAction {
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

bool ActGetProbDescript::int_process(ClientConnection *cc, MessageBlock *mb) {
	string type = (*mb)["type"];
	if(type == "")
		return cc->sendError("You must specify the type to describe");
	
	string desc = ProblemType::getProblemDescription(type);
	if(desc == "")
		return cc->sendError("Error retrieving description for type " + type);

	MessageBlock resp("ok");
	resp["descript"] = desc;

	return cc->sendMessageBlock(&resp);
}

////////////////////////////////////////////////////////////
static ActGetProbTypes _act_get_prob_types;
static ActGetProbDescript _act_get_prob_desc;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "getprobtypes", &_act_get_prob_types);
	ClientAction::registerAction(USER_TYPE_ADMIN, "getprobdescript", &_act_get_prob_desc);
}