/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __TIMEDACTION_H__
#define __TIMEDACTION_H__

#include <time.h>

class TimedAction {
private:
	time_t _time;
public:
	TimedAction(time_t time);
	virtual ~TimedAction();

	time_t processingTime() const;

	virtual void perform() = 0;
};

#endif
