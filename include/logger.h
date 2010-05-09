/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __LOGGER_H__
#define __LOGGER_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <syslog.h>
#include <stdarg.h>

#ifdef __cplusplus
void log(int priority, const char* format, ...) __attribute__ ((format (printf, 2, 3)));
void real_lerror(const char* fname, int line_num, const char* prefix);
void real_log_ssl_errors(const char* fname, int line_num, const char* prefix, unsigned err);
void log_buffer(int priority, const char* name, const unsigned char* buffer, int size);

typedef void (*LogFunction)(int , const char*, va_list ap);
void register_log_listener(LogFunction log_function);

#define lerror(x) real_lerror(__FILE__, __LINE__, x)
#define log_ssl_errors(x) real_log_ssl_errors(__FILE__, __LINE__, x, 0)
#define log_ssl_errors_with_err(x, e) real_log_ssl_errors(__FILE__, __LINE__, x, e)

#define NOT_IMPLEMENTED() log(LOG_DEBUG, "%s (%s:%d) not implemented.", __PRETTY_FUNCTION__, __FILE__, __LINE__)
#else
/* this is needed because of the builtin log() function wich is arithmetic in nature. */
extern void (*logc)(int priority, const char* format, ...);
#endif

#endif
