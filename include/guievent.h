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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <qevent.h>

struct MessageBlock;

class GUIEvent : public QCustomEvent {
private:
	static QWidget* _receiver;

	MessageBlock *_mb;
public:
	explicit GUIEvent(const MessageBlock *mb = NULL);
	virtual ~GUIEvent();

	const MessageBlock *getMB() const { return _mb; }

	void post();

	virtual void process(QWidget* parent) = 0;

	static void setReceiver(QWidget*);
};

#endif
