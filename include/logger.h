#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <syslog.h>
#include <stdarg.h>

#ifdef __cplusplus
void log(int priority, const char* format, ...) __attribute__ ((format (printf, 2, 3)));
void real_lerror(const char* fname, int line_num, const char* prefix);
void real_log_ssl_errors(const char* fname, int line_num, const char* prefix);

#define lerror(x) real_lerror(__FILE__, __LINE__, x)
#define log_ssl_errors(x) real_log_ssl_errors(__FILE__, __LINE__, x)

#define NOT_IMPLEMENTED() log(LOG_DEBUG, "%s (%s:%d) not implemented.", __PRETTY_FUNCTION__, __FILE__, __LINE__)
#else
// this is needed because of the builtin log() function wich is arithmetic in nature.
extern void (*clog)(int priority, const char* format, ...);
#endif

#endif
