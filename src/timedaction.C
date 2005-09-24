#include "timedaction.h"

TimedAction::TimedAction(time_t time) {
	_time = time;
}

TimedAction::~TimedAction() {
}

time_t TimedAction::processingTime() const {
	return _time;
}
