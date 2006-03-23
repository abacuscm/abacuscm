/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __GUIEVENT_H__
#define __GUIEVENT_H__

#include <qevent.h>

class GUIEvent : public QCustomEvent {
private:
	static QWidget* _receiver;
public:
	GUIEvent();
	virtual ~GUIEvent();

	void post();

	virtual void process(QWidget* parent) = 0;

	static void setReceiver(QWidget*);
};

#endif
