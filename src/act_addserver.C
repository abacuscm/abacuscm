#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "logger.h"
#include "message_createserver.h"
#include "server.h"
#include "dbcon.h"

using namespace std;

class ActAddServer : public ClientAction {
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

bool ActAddServer::int_process(ClientConnection* cc, MessageBlock* mb) {
	if(Server::getId() != 1)
		return cc->sendError("Can only add servers from the master server");

	uint32_t user_id = cc->getProperty("user_id");

	string servername = (*mb)["servername"];
	if(servername == "")
		return cc->sendError("Cannot specify an empty server name");

	log(LOG_NOTICE, "User %u requested addition of server '%s'", user_id,
			servername.c_str());

	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Error connecting to database.");
	
	uint32_t server_id = db->name2server_id(servername);
	db->release();

	if(server_id)
		return cc->sendError("Servername is already in use!");

	server_id = Server::nextServerId();
	if(server_id == ~0U)
		return cc->sendError("Unable to determine next server_id");
	
	Message_CreateServer *msg = new Message_CreateServer(servername, server_id);

	MessageHeaders::const_iterator i;
	for(i = mb->begin(); i != mb->end(); ++i) {
		if(i->first != "servername" && i->first != "content-length")
			msg->addAttribute(i->first, i->second);
	}
		
	return triggerMessage(cc, msg);
}

static ActAddServer _act_addserver;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "addserver", &_act_addserver);
}
