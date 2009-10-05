/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "timedaction.h"

TimedAction::TimedAction(time_t time) {
	_time = time;
}

TimedAction::~TimedAction() {
}

time_t TimedAction::processingTime() const {
	return _time;
}
