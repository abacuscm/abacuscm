/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "guievent.h"
#include "messageblock.h"

#include <Qt/qapplication.h>
#include <Qt/qevent.h>

QWidget* GUIEvent::_receiver;

GUIEvent::GUIEvent(const MessageBlock *mb) : QEvent(QEvent::User) {
	/* The callback happens asynchronously after the ServerConnection has
	 * freed the memory, so we need to keep a copy.
	 */
	if (mb)
		_mb = new MessageBlock(*mb);
	else
		_mb = NULL;
}

GUIEvent::~GUIEvent() {
	delete _mb;
}

void GUIEvent::post() {
	QApplication::postEvent(_receiver, this);
}

void GUIEvent::setReceiver(QWidget* receiver) {
	_receiver = receiver;
};
