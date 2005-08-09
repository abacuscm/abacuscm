#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "logger.h"
#include "dbcon.h"
#include "message_type_ids.h"
#include "message.h"

#include <string>

using namespace std;

class ActPasswd : public ClientAction {
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

class PasswdMessage : public Message {
private:
	uint32_t _user_id;
	string _newpass;
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);
public:
	PasswdMessage(uint32_t user_id, const string& newpass);

	virtual bool process() const;

	virtual uint16_t message_type_id() const { return TYPE_ID_UPDATEPASS; }
};


bool ActPasswd::int_process(ClientConnection *cc, MessageBlock *mb) {
	uint32_t user_id = cc->getProperty("user_id");
	string newpass = (*mb)["newpass"];

	return triggerMessage(cc, new PasswdMessage(user_id, newpass));
};

PasswdMessage::PasswdMessage(uint32_t user_id, const string& newpass) {
	_user_id = user_id;
	_newpass = newpass;
}

uint32_t PasswdMessage::storageRequired() {
	return sizeof(_user_id) + _newpass.length() + 1;
}

uint32_t PasswdMessage::store(uint8_t *buffer, uint32_t size) {
	if(size < storageRequired())
		return ~0U;

	char *pos = (char*)buffer;

	*(uint32_t*)pos = _user_id;
	pos += sizeof(uint32_t);

	strcpy(pos, _newpass.c_str());

	return storageRequired();
}

uint32_t PasswdMessage::load(const uint8_t *buffer, uint32_t size) {
	if(size < sizeof(uint32_t))
		return ~0U;
	if(strnlen((char*)buffer + 4, size - sizeof(uint32_t)) == size - sizeof(uint32_t)) {
		return ~0U;
	}

	_user_id = *(uint32_t*)buffer;
	_newpass = (char*)buffer + sizeof(uint32_t);
	
	return storageRequired();		
}

bool PasswdMessage::process() const {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return false;
	return db->setPassword(_user_id, _newpass);
}

static ActPasswd _act_passwd;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "passwd", &_act_passwd);
	ClientAction::registerAction(USER_TYPE_JUDGE, "passwd", &_act_passwd);
	ClientAction::registerAction(USER_TYPE_PARTICIPANT, "passwd", &_act_passwd);
	ClientAction::registerAction(USER_TYPE_MARKER, "passwd", &_act_passwd);
}
