#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <queue>

template<class T>
class Queue {
private:
	std::queue<T> _q; // queue.
	pthread_mutex_t _m; // mutex.
	pthread_mutex_t _e; // empty lock.
public:
	Queue() {
		pthread_mutex_init(&_m, NULL);
		pthread_mutex_init(&_e, NULL);
		pthread_mutex_lock(&_e);
	}

	~Queue() {
		pthread_mutex_destroy(&_m);
	}

	T dequeue() {
		// I'm sure we can invent some fancy do {} while() but there
		// is no correct test for whether we correctly dequeue'ed
		// something :).  Alternatively we could use tail-recursion
		// but that could potentially stack-overflow.  Unless the
		// compiler is smart enough.
retry:
		pthread_mutex_lock(&_m);
		if(_q.empty()) {
			pthread_mutex_unlock(&_m);
			pthread_mutex_lock(&_e);
			goto retry;
		}
		T t = _q.front();
		_q.pop();
		pthread_mutex_unlock(&_m);
		return t;
	}

	void enqueue(T v) {
		pthread_mutex_lock(&_m);
		if(_q.empty())
			pthread_mutex_unlock(&_e);
		_q.push(v);
		pthread_mutex_unlock(&_m);
	}
};

#endif
