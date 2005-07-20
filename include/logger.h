#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <syslog.h>
#include <stdarg.h>

#ifdef __cplusplus
void log(int priority, const char* format, ...) __attribute__ ((format (printf, 2, 3)));
void lerror(const char* prefix);

#define NOT_IMPLEMENTED() log(LOG_DEBUG, "%s (%s:%d) not implemented.", __PRETTY_FUNCTION__, __FILE__, __LINE__)
#else
// this is needed because of the builtin log() function wich is arithmetic in nature.
extern void (*clog)(int priority, const char* format, ...);
#endif

#endif
