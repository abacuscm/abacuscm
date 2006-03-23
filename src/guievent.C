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

#include <qapplication.h>
#include <qevent.h>

QWidget* GUIEvent::_receiver;

GUIEvent::GUIEvent() : QCustomEvent(QEvent::User) {
}

GUIEvent::~GUIEvent() {
}

void GUIEvent::post() {
	QApplication::postEvent(_receiver, this);
}

void GUIEvent::setReceiver(QWidget* receiver) {
	_receiver = receiver;
};
