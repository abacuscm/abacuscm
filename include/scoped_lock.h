/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __SCOPED_LOCK_H__
#define __SCOPED_LOCK_H__

#include <pthread.h>

class ScopedLock {
private:
	pthread_mutex_t *_lock;
public:
	ScopedLock(pthread_mutex_t *lock) : _lock(lock) { pthread_mutex_lock(_lock); }
	~ScopedLock() { pthread_mutex_unlock(_lock); }
};

#endif
