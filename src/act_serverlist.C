#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "dbcon.h"

#include <sstream>

class ServerListAct : public ClientAction {
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

bool ServerListAct::int_process(ClientConnection *cc, MessageBlock *) {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return cc->sendError("Error connecting to database");

	ServerList list = db->getServers();
	db->release();db=NULL;

	MessageBlock mb("ok");
	int c = 0;
	for(ServerList::iterator i = list.begin(); i != list.end(); ++i, ++c) {
		std::ostringstream t;
		t << "server" << c;
		mb[t.str()] = i->second;
	}
	
	return cc->sendMessageBlock(&mb);
}

static ServerListAct _act_servlist;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "getserverlist", &_act_servlist);
}
