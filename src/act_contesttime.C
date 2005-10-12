#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "server.h"

class ActContesttime : public ClientAction {
protected:
	virtual bool int_process(ClientConnection* cc, MessageBlock*mb);
};

bool ActContesttime::int_process(ClientConnection* cc, MessageBlock*) {
	uint32_t contesttime = Server::contestTime();
	bool running = Server::isContestRunning();

	if(contesttime > 5 * 60 * 60)
		contesttime = 5 * 60 * 60;

	char buffer[20];
	sprintf(buffer, "%u", contesttime);

	MessageBlock res("ok");
	res["time"] = buffer;
	res["running"] = running ? "yes" : "no";

	return cc->sendMessageBlock(&res);
}

static ActContesttime _act_contesttime;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "contesttime", &_act_contesttime);
	ClientAction::registerAction(USER_TYPE_JUDGE, "contesttime", &_act_contesttime);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "contesttime", &_act_contesttime);
}
