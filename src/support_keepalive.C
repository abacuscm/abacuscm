/**
 * Copyright (c) 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */

#include "acmconfig.h"
#include "logger.h"
#include "supportmodule.h"
#include "timedaction.h"
#include "server.h"
#include "clienteventregistry.h"
#include "messageblock.h"
#include "misc.h"
#include <time.h>

class KeepaliveSupportModule : public SupportModule {
	DECLARE_SUPPORT_MODULE(KeepaliveSupportModule);

private:
	class KeepaliveTimedAction : public TimedAction {
	public:
		virtual void perform();
		KeepaliveTimedAction(time_t time) : TimedAction(time) {}
	};

	KeepaliveSupportModule();
	virtual ~KeepaliveSupportModule();

public:
	virtual void init();
};

void KeepaliveSupportModule::KeepaliveTimedAction::perform() {
	Server::putTimedAction(new KeepaliveTimedAction(processingTime() + KEEPALIVE_INTERVAL));

	MessageBlock keepalive("keepalive");
	ClientEventRegistry::getInstance().broadcastEvent(
		0, PermissionTest::ANY, &keepalive);
}

DEFINE_SUPPORT_MODULE(KeepaliveSupportModule);

KeepaliveSupportModule::KeepaliveSupportModule() {
}

KeepaliveSupportModule::~KeepaliveSupportModule() {
}

void KeepaliveSupportModule::init() {
	Server::putTimedAction(new KeepaliveTimedAction(time(NULL) + KEEPALIVE_INTERVAL));
}
