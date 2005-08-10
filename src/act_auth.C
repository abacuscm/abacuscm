#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "logger.h"
#include "dbcon.h"

class ActAuth : public ClientAction {
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

bool ActAuth::int_process(ClientConnection *cc, MessageBlock *mb) {
	DbCon *db = DbCon::getInstance();
	if(!db) {
		cc->sendError("Unable to connect to database.");
		return false;
	}

	uint32_t user_id;
	uint32_t user_type;
	int result = db->authenticate((*mb)["user"], (*mb)["pass"], &user_id, &user_type);
	db->release();
	
	if(result < 0)
		cc->sendError("Database error.");
	else if(result == 0)
		cc->sendError("Authentication failed.  Invalid username and/or password.");
	else {
		log(LOG_INFO, "User '%s' successfully logged in.", (*mb)["user"].c_str());
		cc->setProperty("user_id", user_id);
		cc->setProperty("user_type", user_type);
		cc->reportSuccess();
	}
	return result > 0;
}

static ActAuth _act_auth;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_NONE, "auth", &_act_auth);
}