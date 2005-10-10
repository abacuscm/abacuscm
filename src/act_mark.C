#include "clientaction.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "logger.h"
#include "markers.h"

class ActSubscribeMark : public ClientAction {
protected:
	virtual bool int_process(ClientConnection*, MessageBlock* mb);
};

class ActPlaceMark : public ClientAction {
protected:
	virtual bool int_process(ClientConnection*, MessageBlock* mb);
};

bool ActSubscribeMark::int_process(ClientConnection* cc, MessageBlock*) {
	Markers::getInstance().enqueueMarker(cc);
	return cc->reportSuccess();
}

bool ActPlaceMark::int_process(ClientConnection* cc, MessageBlock*mb) {
	return Markers::getInstance().putMark(cc, mb);
}

static ActSubscribeMark _act_subscribe_mark;
static ActPlaceMark _act_place_mark;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_MARKER, "subscribemark", &_act_subscribe_mark);
	ClientAction::registerAction(USER_TYPE_MARKER, "putmark", &_act_place_mark);
}
