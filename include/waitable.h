/**
 * Copyright (c) 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */

#ifndef __WAITABLE_H__
#define __WAITABLE_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <vector>
#include <pthread.h>
#include <poll.h>

/* Interface for objects that can be waited on with a poll()
 * call - i.e. they provide a fd that can be waited for.
 */
class Waitable {
private:
	/* Lock used to protect access to _events.
	 */
	mutable pthread_mutex_t _lock;

	/* Events to return from getEvents. This is cleared
	 * during processing, then set to the result.
	 */
	std::vector<pollfd> _events;

	friend class WaitableSet;

private:
	// Prevent copying
	Waitable(const Waitable &w);
	Waitable &operator =(const Waitable &w);

	void lock() const;
	void unlock() const;

	// Used internally only. These functions are thread-safe.
	void setEvents(const std::vector<pollfd> &events);
	// Called by a WaitableSet to determine which events to wait for.
	// Appends the events to the existing set.
	void getEvents(std::vector<pollfd> &events) const;
	// Checks whether there are any more events to wait for
	bool isFinished() const;

protected:
	/* Internal implementation of process, provided by subclasses.
	 *
	 * Returns a vector of pollfd events to wait for. If the return value is
	 * empty, it indicates to the caller that the Waitable should be deleted.
	 */
	virtual std::vector<pollfd> int_process() = 0;

public:
	Waitable();
	virtual ~Waitable();

	/* Populate with the initial set of events to watch. This must be called
	 * before adding the Waitable to a WaitableSet.
	 */
	void initialise(const std::vector<pollfd> &events);

	/* Called before process(). Unlike process(), this is guaranteed to be
	 * called synchronously with the WaitableSet main loop, and so needs to
	 * complete quickly.
	 */
	void preprocess();

	/* Called when one of the events returned by getEvents() is signalled.
	 */
	void process();
};

/* Abstract class for monitoring a set of Waitable objects and processing them.
 * The method of processing (e.g., thread pool, in-thread, spawned thread) is
 * up to the concrete subclasses.
 */
class WaitableSet {
private:
	std::vector<Waitable *> _waitables;

	/* Pipe pair that is used to force wakeup. The characters
	 * written to it are irrelevant.
	 */
	int _wakeup_fd[2];

	bool _shutdown;

	pthread_mutex_t _lock; /* Protects _waitables and _shutdown */

	friend class Waitable;

private:
	// Prevent copying
	WaitableSet(const WaitableSet &);
	WaitableSet &operator =(const WaitableSet &);

	// Interrupts the poll and recomputes pollfd set
	void wakeup();

	void lock();
	void unlock();

protected:
	// Called by subclass after processing
	void process_finish(Waitable *w);

	/* Start process a signalled waitable, possibly asynchronously. The
	 * implementation must
	 * 1. Call w->preprocess() before returning.
	 * 2. Call w->process(), possibly in another thread.
	 * 3. Call process_finish(w), possibly in another thread.
	 */
	virtual void process(Waitable *w) = 0;

public:
	WaitableSet();
	virtual ~WaitableSet();

	/* Add to the set; automatically interrupts existing wait */
	void add(Waitable *w);

	/* Runs main loop until a shutdown command is received. */
	virtual void run();

	/* Enqueue a shutdown command to the main loop */
	void shutdown();

	/* Deletes all attached Waitables. Note: if the subclass does asynchronous
	 * processing, the caller must ensure that none of them are being
	 * processed.
	 */
	void delete_all();
};

#endif /* __WAITABLE_H__ */
