#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "logger.h"
#include "dbcon.h"
#include "eventregister.h"

class ActWhatAmI : public ClientAction {
private:
	std::map<uint32_t, std::string> _typemap;
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *);
public:
	ActWhatAmI();
};

ActWhatAmI::ActWhatAmI() {
	_typemap[USER_TYPE_NONE] = "none";
	_typemap[USER_TYPE_ADMIN] = "admin";
	_typemap[USER_TYPE_JUDGE] = "judge";
	_typemap[USER_TYPE_CONTESTANT] = "contestant";
}

bool ActWhatAmI::int_process(ClientConnection *cc, MessageBlock *) {
	uint32_t user_type = cc->getProperty("user_type");

	MessageBlock resp("ok");
	resp["type"] = _typemap[user_type];
	return cc->sendMessageBlock(&resp);
}

static ActWhatAmI _act_whatami;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_NONE, "whatami", &_act_whatami);
	ClientAction::registerAction(USER_TYPE_ADMIN, "whatami", &_act_whatami);
	ClientAction::registerAction(USER_TYPE_JUDGE, "whatami", &_act_whatami);
	ClientAction::registerAction(USER_TYPE_CONTESTANT, "whatami", &_act_whatami);
}