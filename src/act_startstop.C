#include "clientaction.h"
#include "message.h"
#include "message_type_ids.h"
#include "logger.h"
#include "server.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "dbcon.h"

#include <string>

using namespace std;

class ActStartStop : public ClientAction {
protected:
	virtual bool int_process(ClientConnection*, MessageBlock*);
};

#define ACT_STOP	1
#define ACT_START	2

class MsgStartStop : public Message {
private:
	uint32_t _server_id;
	uint32_t _time;
	uint32_t _action; // way too big...
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);
public:
	MsgStartStop();
	MsgStartStop(uint32_t server_id, uint32_t time, uint32_t action);

	virtual bool process() const;

	virtual uint16_t message_type_id() const;
};

bool ActStartStop::int_process(ClientConnection* cc, MessageBlock *mb) {
	char *errpnt;

	uint32_t server_id;
	uint32_t time;
	uint32_t action;

	string t_action = (*mb)["action"];
	if(t_action == "start") {
		action = ACT_START;
	} else if(t_action == "stop") {
		action = ACT_STOP;
	} else
		return cc->sendError("Invalid 'action', must be either 'start' or 'stop'");

	time = strtoll((*mb)["time"].c_str(), &errpnt, 0);
	if(time == 0 || *errpnt)
		return cc->sendError("Invalid start/stop time specified");

	// note that an omitted or blank server_id translates to 0, meaning
	// "all" servers.
	server_id = strtoll((*mb)["server_id"].c_str(), &errpnt, 0);
	if(*errpnt)
		return cc->sendError("Invalid server_id specified");

	return triggerMessage(cc, new MsgStartStop(server_id, time, action));
}
	
MsgStartStop::MsgStartStop() {
}
	
MsgStartStop::MsgStartStop(uint32_t server_id, uint32_t time, uint32_t action) {
	_server_id = server_id;
	_time = time;
	_action = action;
}

uint32_t MsgStartStop::storageRequired() {
	return 3 * sizeof(uint32_t);
}

uint32_t MsgStartStop::store(uint8_t *buffer, uint32_t size) {
	if(size < 3 * sizeof(uint32_t))
		return ~0U;

	uint32_t *pos = (uint32_t*)buffer;
	*pos++ = _server_id;
	*pos++ = _time;
	*pos++ = _action;
	return 3 * sizeof(uint32_t);
}

uint32_t MsgStartStop::load(const uint8_t *buffer, uint32_t size) {
	if(size < 3 * sizeof(uint32_t))
		return ~0U;

	const uint32_t *pos = (const uint32_t*)buffer;
	_server_id = *pos++;
	_time = *pos++;
	_action = *pos++;

	log(LOG_DEBUG, "MsgStartStop: %d %d %d", _server_id, _time, _action);
	return 3 * sizeof(uint32_t);
}

bool MsgStartStop::process() const {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return false;

	bool dbres = db->startStopContest(_server_id, _time, _action == ACT_START);
	db->release();

	return dbres;
}

uint16_t MsgStartStop::message_type_id() const {
	return TYPE_ID_STARTSTOP;
}

static ActStartStop _act_startstop;

static Message* StartStopFunctor() {
	return new MsgStartStop();
}

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "startstop", &_act_startstop);
	Message::registerMessageFunctor(TYPE_ID_STARTSTOP, StartStopFunctor);
}