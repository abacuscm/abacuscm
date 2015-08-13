/**
 * Copyright (c) 2015 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 */
#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "logger.h"
#include "dbcon.h"
#include "message.h"
#include "message_type_ids.h"
#include "standingssupportmodule.h"
#include "usersupportmodule.h"

#include <sstream>
#include <memory>

using namespace std;

class ActSetbonus : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

class ActGetbonus : public ClientAction {
protected:
	virtual auto_ptr<MessageBlock> int_process(ClientConnection *cc, const MessageBlock *mb);
};

class BonusMessage : public Message {
private:
	uint32_t _user_id;
	int32_t _points, _seconds;
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);

	virtual bool int_process() const;
public:
	BonusMessage();
	BonusMessage(uint32_t user_id, int32_t points, int32_t seconds);

	virtual uint16_t message_type_id() const { return TYPE_ID_SETBONUS; }
};

auto_ptr<MessageBlock> ActSetbonus::int_process(ClientConnection *cc, const MessageBlock *mb) {
	uint32_t user_id;
	int32_t points, seconds;
	char *errptr;

	user_id = strtoul((*mb)["user_id"].c_str(), &errptr, 0);
	if(*errptr || (*mb)["user_id"] == "")
		return MessageBlock::error("user_id isn't a valid integer");

	points = strtol((*mb)["points"].c_str(), &errptr, 0);
	if (*errptr || (*mb)["points"] == "")
		return MessageBlock::error("points isn't a valid integer");

	seconds = strtol((*mb)["seconds"].c_str(), &errptr, 0);
	if (*errptr || (*mb)["seconds"] == "")
		return MessageBlock::error("seconds isn't a valid integer");

	return triggerMessage(cc, new BonusMessage(user_id, points, seconds));
}

auto_ptr<MessageBlock> ActGetbonus::int_process(ClientConnection *, const MessageBlock *mb) {
	uint32_t user_id;
	int32_t points, seconds;
	char *errptr;

	user_id = strtoul((*mb)["user_id"].c_str(), &errptr, 0);
	if(*errptr || (*mb)["user_id"] == "")
		return MessageBlock::error("user_id isn't a valid integer");

	UserSupportModule *usm = getUserSupportModule();
	if (!usm->user_bonus(user_id, points, seconds))
		return MessageBlock::error("could not get bonus for this user");

	ostringstream os;
	auto_ptr<MessageBlock> res(MessageBlock::ok());
	os << points;
	(*res)["points"] = os.str();
	os.str("");
	os << seconds;
	(*res)["seconds"] = os.str();

	return res;
}

BonusMessage::BonusMessage() {}

BonusMessage::BonusMessage(uint32_t user_id, int32_t points, int32_t seconds) {
	_user_id = user_id;
	_points = points;
	_seconds = seconds;
}

uint32_t BonusMessage::storageRequired() {
	return sizeof(_user_id) + sizeof(_points) + sizeof(_seconds);
}

uint32_t BonusMessage::store(uint8_t *buffer, uint32_t size) {
	if(size < storageRequired())
		return ~0U;

	memcpy(buffer, &_user_id, sizeof(_user_id));
	buffer += sizeof(_user_id);
	memcpy(buffer, &_points, sizeof(_points));
	buffer += sizeof(_points);
	memcpy(buffer, &_seconds, sizeof(_seconds));
	buffer += sizeof(_seconds);

	return storageRequired();
}

uint32_t BonusMessage::load(const uint8_t *buffer, uint32_t size) {
	if (size < storageRequired())
		return ~0U;

	memcpy(&_user_id, buffer, sizeof(_user_id));
	buffer += sizeof(_user_id);
	memcpy(&_points, buffer, sizeof(_points));
	buffer += sizeof(_user_id);
	memcpy(&_seconds, buffer, sizeof(_seconds));
	buffer += sizeof(_seconds);

	return storageRequired();
}

bool BonusMessage::int_process() const {
	DbCon *db = DbCon::getInstance();
	if(!db)
		return false;

	stringstream query;
	query << "UPDATE User SET bonus_points=" << _points
		<< ", bonus_seconds=" << _seconds << " WHERE user_id=" << _user_id;
	bool result = db->executeQuery(query.str());
	db->release();

	StandingsSupportModule *standings = getStandingsSupportModule();
	if(standings)
		standings->updateStandings(_user_id, 0);
	return result;
}

static ActGetbonus _act_getbonus;
static ActSetbonus _act_setbonus;

static Message* create_bonus_msg() {
	return new BonusMessage();
}

extern "C" void abacuscm_mod_init() {
	ClientAction::registerAction("getbonus", PERMISSION_SEE_STANDINGS, &_act_getbonus);
	ClientAction::registerAction("setbonus", PERMISSION_SET_BONUS, &_act_setbonus);
	Message::registerMessageFunctor(TYPE_ID_SETBONUS, create_bonus_msg);
}
