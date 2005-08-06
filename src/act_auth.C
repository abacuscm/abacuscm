#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "logger.h"

class ActAuth : public ClientAction {
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

bool ActAuth::int_process(ClientConnection *cc, MessageBlock *mb) {
	log(LOG_DEBUG, "Attempt from user '%s' to log in with password '%s'.",
			(*mb)["user"].c_str(), (*mb)["pass"].c_str());
	return true;
}

static ActAuth _act_auth;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_NONE, "auth", &_act_auth);
}
