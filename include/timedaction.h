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
