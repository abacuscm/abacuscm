#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
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

bool ClientAction::process(ClientConnection *cc, MessageBlock *mb) {
	int user_type = cc->getUserType();
	std::string action = mb->action();

	ClientAction* ca = actionmap[user_type][action];
	if(ca)
		return ca->int_process(cc, mb);
	else {
		log(LOG_NOTICE, "Unknown action '%s' encountered on ClientConnection for user %d of type %d.", action.c_str(), -1, user_type);
		return false;
	}
}
