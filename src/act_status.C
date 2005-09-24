#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "server.h"

class ActRunning : public ClientAction {
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

bool ActRunning::int_process(ClientConnection *cc, MessageBlock*) {
	MessageBlock mb("ok");
	if(Server::isContestRunning())
		mb["status"] = "running";
	else
		mb["status"] = "stopped";

	return cc->sendMessageBlock(&mb);
}

static ActRunning _act_running;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "conteststatus", &_act_running);
	ClientAction::registerAction(USER_TYPE_JUDGE, "conteststatus", &_act_running);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "conteststatus", &_act_running);
}
