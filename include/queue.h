#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <queue>
#include <list>

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
};

#endif
