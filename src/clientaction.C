#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "message.h"
#include "logger.h"

std::map<int, std::map<std::string, ClientAction*> > ClientAction::actionmap;

ClientAction::ClientAction() {
}

ClientAction::~ClientAction() {
}


bool ClientAction::registerAction(int user_type, std::string action, ClientAction *ca) {
	if(actionmap[user_type][action] != NULL)
		log(LOG_WARNING, "Overriding action handler for action '%s'!", action.c_str());

	actionmap[user_type][action] = ca;
	return true;
}

bool ClientAction::triggerMessage(ClientConnection *cc, Message *mb) {
	if(mb->makeMessage()) {
		if(mb->process()) {
			cc->reportSuccess();
			return true;
		} else
			cc->sendError("Error processing resulting server message. This is indicative of a bug.");
	} else
		cc->sendError("Internal error creating message.  This is indicative of a bug.");
	return false;
}

bool ClientAction::process(ClientConnection *cc, MessageBlock *mb) {
	int user_type = cc->getProperty("user_type");
	std::string action = mb->action();

	ClientAction* ca = actionmap[user_type][action];
	if(ca)
		return ca->int_process(cc, mb);
	else {
		log(LOG_NOTICE, "Unknown action '%s' encountered on ClientConnection for user %d of type %d.", action.c_str(), -1, user_type);
		cc->sendError("No such action");
		return true;
	}
}
