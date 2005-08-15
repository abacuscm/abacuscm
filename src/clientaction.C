#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "message.h"
#include "logger.h"

std::map<int, std::map<std::string, ClientAction*> > ClientAction::actionmap;
Queue<Message*> *ClientAction::_message_queue;

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
		_message_queue->enqueue(mb);
		return cc->reportSuccess();
	} else
		return cc->sendError("Internal error creating message.  This is indicative of a bug.");
}

bool ClientAction::process(ClientConnection *cc, MessageBlock *mb) {
	uint32_t user_type = cc->getProperty("user_type");
	std::string action = mb->action();

	ClientAction* ca = actionmap[user_type][action];
	if(ca)
		return ca->int_process(cc, mb);
	else {
		uint32_t user_id = cc->getProperty("user_id");
		log(LOG_NOTICE, "Unknown action '%s' encountered on ClientConnection for user %u of type %u.", action.c_str(), user_id, user_type);
		cc->sendError("No such action");
		return true;
	}
}

void ClientAction::setMessageQueue(Queue<Message*> *message_queue) {
	_message_queue = message_queue;
}
