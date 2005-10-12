#include "clientaction.h"
#include "clientconnection.h"
#include "logger.h"
#include "message.h"
#include "messageblock.h"
#include "message_type_ids.h"
#include "dbcon.h"
#include "server.h"
#include "eventregister.h"
#include "markers.h"

#include <sstream>
#include <time.h>

class ActGetClarifications : public ClientAction {
protected:
	bool int_process(ClientConnection *cc, MessageBlock *mb);
};

bool ActGetClarifications::int_process(ClientConnection *cc, MessageBlock *) {
	DbCon *db = DbCon::getInstance();
	if (!db)
		return cc->sendError("Error connecting to database");

	uint32_t uid = cc->getProperty("user_id");
	uint32_t utype = cc->getProperty("user_type");

	ClarificationList lst;

	if(utype == USER_TYPE_CONTESTANT)
		lst = db->getClarifications(uid);
	else
		lst = db->getClarifications();

	MessageBlock mb("ok");

	ClarificationList::iterator s;
	int c = 0;
	for (s = lst.begin(); s != lst.end(); ++s, ++c) {
		std::ostringstream tmp;
		tmp << c;
		std::string cntr = tmp.str();

		AttributeList::iterator a;
		for(a = s->begin(); a != s->end(); ++a)
			mb[a->first + cntr] = a->second;
	}

	return cc->sendMessageBlock(&mb);
}

class ActGetClarificationRequests : public ClientAction {
protected:
	bool int_process(ClientConnection *cc, MessageBlock *mb);
};

bool ActGetClarificationRequests::int_process(ClientConnection *cc, MessageBlock *) {
	DbCon *db = DbCon::getInstance();
	if (!db)
		return cc->sendError("Error connecting to database");

	uint32_t uid = cc->getProperty("user_id");
	uint32_t utype = cc->getProperty("user_type");

	ClarificationRequestList lst;

	if(utype == USER_TYPE_CONTESTANT)
		lst = db->getClarificationRequests(uid);
	else
		lst = db->getClarificationRequests();

	MessageBlock mb("ok");

	ClarificationRequestList::iterator s;
	int c = 0;
	for (s = lst.begin(); s != lst.end(); ++s, ++c) {
		std::ostringstream tmp;
		tmp << c;
		std::string cntr = tmp.str();

		AttributeList::iterator a;
		for(a = s->begin(); a != s->end(); ++a)
			mb[a->first + cntr] = a->second;
	}

	return cc->sendMessageBlock(&mb);
}


static ActGetClarifications _act_getclarifications;
static ActGetClarificationRequests _act_getclarificationrequests;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "getclarifications", &_act_getclarifications);
	ClientAction::registerAction(USER_TYPE_JUDGE, "getclarifications", &_act_getclarifications);
	ClientAction::registerAction(USER_TYPE_ADMIN, "getclarifications", &_act_getclarifications);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "getclarificationrequests", &_act_getclarificationrequests);
	ClientAction::registerAction(USER_TYPE_JUDGE, "getclarificationrequests", &_act_getclarificationrequests);
	ClientAction::registerAction(USER_TYPE_ADMIN, "getclarificationrequests", &_act_getclarificationrequests);
}
