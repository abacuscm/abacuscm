#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <syslog.h>
#include <stdarg.h>

void log(int priority, const char* format, ...) __attribute__ ((format (printf, 2, 3)));
void lerror(const char* prefix);

#define NOT_IMPLEMENTED() log(LOG_DEBUG, "%s (%s:%d) not implemented.", __PRETTY_FUNCTION__, __FILE__, __LINE__)

#endif
