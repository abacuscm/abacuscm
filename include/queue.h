/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __QUEUE_H__
#define __QUEUE_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <queue>
#include <list>
#include <sys/time.h>
#include <errno.h>

template<typename T>
class Queue {
private:
	std::queue<T> _q; // queue.
	pthread_mutex_t _m; // mutex.
	pthread_cond_t _e; // empty condition.
public:
	Queue() {
		pthread_mutex_init(&_m, NULL);
		pthread_cond_init(&_e, NULL);
	}

	~Queue() {
		pthread_mutex_destroy(&_m);
		pthread_cond_destroy(&_e);
	}

	T dequeue() {
		pthread_mutex_lock(&_m);
		while(_q.empty())
			pthread_cond_wait(&_e, &_m);
		T t = _q.front();
		_q.pop();
		pthread_mutex_unlock(&_m);
		return t;
	}

	T timed_dequeue(int sec, T def) {
		struct timeval now;
		struct timespec timeout;
		int retcode = 0;
		T t = def;

		gettimeofday(&now, NULL);
		timeout.tv_sec = now.tv_sec + sec;
		timeout.tv_nsec = now.tv_usec * 1000;

		pthread_mutex_lock(&_m);
		while(_q.empty() && retcode != ETIMEDOUT)
			retcode = pthread_cond_timedwait(&_e, &_m, &timeout);

		if(retcode != ETIMEDOUT) {
			t = _q.front();
			_q.pop();
		}
		pthread_mutex_unlock(&_m);
		return t;
	}

	void enqueue(T v) {
		pthread_mutex_lock(&_m);
		pthread_cond_signal(&_e);
		_q.push(v);
		pthread_mutex_unlock(&_m);
	}

	template<typename _InputIterator>
	void enqueue(_InputIterator __first, _InputIterator __last) {
		pthread_mutex_lock(&_m);
		pthread_cond_signal(&_e);
		while(__first != __last) {
			_q.push(*__first);
			++__first;
		}
		pthread_mutex_unlock(&_m);
	}

	bool empty() {
		pthread_mutex_lock(&_m);
		bool result = _q.empty();
		pthread_mutex_unlock(&_m);
		return result;
	}
};

#endif
