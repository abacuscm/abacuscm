#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "server.h"

#include <sstream>

class ActContesttime : public ClientAction {
protected:
	virtual bool int_process(ClientConnection* cc, MessageBlock*mb);
};

bool ActContesttime::int_process(ClientConnection* cc, MessageBlock*) {
	uint32_t contesttime = Server::contestTime();
	uint32_t contestremain = Server::contestRemaining();
	bool running = Server::isContestRunning();

	std::ostringstream os;

	MessageBlock res("ok");
	res["running"] = running ? "yes" : "no";

	os << contesttime;
	res["time"] = os.str();

	os.str("");
	os << contestremain;
	res["remain"] = os.str();

	return cc->sendMessageBlock(&res);
}

static ActContesttime _act_contesttime;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "contesttime", &_act_contesttime);
	ClientAction::registerAction(USER_TYPE_JUDGE, "contesttime", &_act_contesttime);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "contesttime", &_act_contesttime);
}
