/**
 * Copyright (c) 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <pthread.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <vector>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include "logger.h"
#include "waitable.h"

using namespace std;

Waitable::Waitable() {
	pthread_mutex_init(&_lock, NULL);
}

Waitable::~Waitable() {
	pthread_mutex_destroy(&_lock);
}

void Waitable::initialise(const std::vector<pollfd> &events) {
	// Check that it isn't already initialised
	assert(_events.empty());
	_events = events;
}

void Waitable::lock() const {
	pthread_mutex_lock(&_lock);
}

void Waitable::unlock() const {
	pthread_mutex_unlock(&_lock);
}

void Waitable::setEvents(const std::vector<pollfd> &events) {
	lock();
	_events = events;
	unlock();
}

void Waitable::getEvents(std::vector<pollfd> &events) const {
	lock();
	events.insert(events.end(), _events.begin(), _events.end());
	unlock();
}

bool Waitable::isFinished() const {
	lock();
	bool ans = _events.empty();
	unlock();
	return ans;
}

void Waitable::preprocess() {
	// Don't wait for this waitable while it is still being processed
	lock();
	_events.clear();
	unlock();
}

void Waitable::process() {
	std::vector<pollfd> new_events = int_process();
	setEvents(new_events);
}


WaitableSet::WaitableSet() {
	_shutdown = false;
	// TODO error check
	pthread_mutex_init(&_lock, NULL);
	// TODO error check
	pipe(_wakeup_fd);
	if (fcntl(_wakeup_fd[0], F_SETFL, O_NONBLOCK) < 0) {
		lerror("fcntl");
		log(LOG_INFO, "Continuing anyway ...");
	}
	if (fcntl(_wakeup_fd[1], F_SETFL, O_NONBLOCK) < 0) {
		lerror("fcntl");
		log(LOG_INFO, "Continuing anyway ...");
	}
}

void WaitableSet::delete_all() {
	for (size_t i = 0; i < _waitables.size(); i++)
		delete _waitables[i];
	_waitables.clear();
}

WaitableSet::~WaitableSet() {
	delete_all();
	pthread_mutex_destroy(&_lock);
	close(_wakeup_fd[0]);
	close(_wakeup_fd[1]);
}

void WaitableSet::lock() {
	pthread_mutex_lock(&_lock);
}

void WaitableSet::unlock() {
	pthread_mutex_unlock(&_lock);
}

void WaitableSet::wakeup() {
	char dummy = 0;
	// TODO error check
	/* Note: don't need to check for pipe full or EPIPE,
	 * because in that case the poll will be interrupted
	 * anyway.
	 */
	write(_wakeup_fd[1], &dummy, 1);
}

void WaitableSet::shutdown() {
	lock();
	_shutdown = true;
	unlock();
	wakeup();
}

void WaitableSet::process_finish(Waitable *w) {
	if (w->isFinished()) {
		// Done with this one, so delete it
		lock();
		vector<Waitable *>::iterator pos = remove(_waitables.begin(), _waitables.end(), w);
		_waitables.erase(pos, _waitables.end());
		unlock();
		delete w;
	}
	wakeup();
}

void WaitableSet::add(Waitable *w) {
	assert(w != NULL);
	// If this fails, either w is being processed, or has been created with
	// no initial events
	assert(!w->isFinished());
	lock();
	assert(count(_waitables.begin(), _waitables.end(), w) == 0);
	_waitables.push_back(w);
	unlock();
	wakeup();
}

void WaitableSet::run() {
	vector<pollfd> pollfds;
	vector<Waitable *> active;

	pollfd wakeup_pollfd;
	wakeup_pollfd.fd = _wakeup_fd[0];
	wakeup_pollfd.events = POLLIN;
	wakeup_pollfd.revents = 0;
	while (true) {
		lock();
		bool do_shutdown = _shutdown;
		pollfds.clear();
		active.clear();
		for (size_t i = 0; i < _waitables.size(); i++) {
			Waitable *w = _waitables[i];
			w->getEvents(pollfds);
			// Pads active up with copies of w to match the length of pollfds
			active.resize(pollfds.size(), w);
		}
		unlock();
		if (do_shutdown)
			return;

		pollfds.push_back(wakeup_pollfd);
		int status = poll(&pollfds[0], pollfds.size(), -1);
		if (status < 0) {
			if (errno != EINTR)
				lerror("poll");
		} else {
			/* Drain the wakeup pipe */
			char buffer[64];
			while (read(_wakeup_fd[0], buffer, sizeof(buffer)) > 0) {
				// Do nothing
			}

			Waitable *last = NULL;
			/* Process any waitables that have activity */
			for (size_t i = 0; i < active.size(); i++) {
				if (active[i] != last && pollfds[i].revents != 0)
				{
					last = active[i];
					process(last);
				}
			}
		}
	}
}
