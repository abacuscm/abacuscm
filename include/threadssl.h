/*
 * Copyright (c) 2009 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */

/* Routines to allow thread-safe access to an SSL connection - in particular,
 * servicing the read and write ends from independent threads. SSL is not
 * thread-safe when calling multiple routines on the same connection
 * simultaneously.
 *
 * The basic approach is to use non-blocking I/O under the hood, and to
 * simulate blocking I/O on top of it using poll where required. The lock
 * around the actual access is dropped while polling, so that the waiting
 * will not prevent the other direction from proceeding if possible.
 */

#ifndef __THREADSSL_H__
#define __THREADSSL_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <openssl/ssl.h>
#include <pthread.h>
#include <sys/types.h>

class ThreadSSL {
public:
	enum Mode {
		NONBLOCK,       // never block
		BLOCK_PARTIAL,  // block until there is some data, but allow partial amounts
		BLOCK_FULL      // block until everything is there
	};

	struct Status {
		int processed;  // bytes read/written
		int err;        // value of SSL_get_error(_ssl, ret)
	};

private:
	// Protects all class members, as well as simultaneous calls to SSL
	pthread_mutex_t _lock;
	SSL * _ssl;
	bool _shutting_down;
	// Pipe pair written to notify that startShutdown has been called
	int _shutdown_notify_read, _shutdown_notify_write;

	// Prevent copying
	ThreadSSL(const ThreadSSL *);
	ThreadSSL &operator=(const ThreadSSL *);

	// Bottom half of all wrapper functions
	// Returns true if the caller should return, false to go round the loop
	// The status is both read and modified
	bool postProcess(Status &stat, int fd, int len, Mode mode);

public:
	/* Call before using any SSL functions in a multithreaded environment */
	static void initialise();

	/* Reverse effects of initialise() */
	static void cleanup();

	/* This should be a non-blocking SSL connection. However, if this object
	 * is only going to be used from one thread at a time it doesn't have to
	 * be - but NONBLOCK mode will nevertheless block, and only one call
	 * to a read/write/etc function can be active at a time.
	 */
	explicit ThreadSSL(SSL *ssl);

	/* Shuts down the connection and frees the SSL object. At the time this
	 * is called, there must be no other threads using this object - for a
	 * controlled shutdown, use start_shutdown, then wait for those threads to
	 * finish up, then call this destructor.
	 */
	~ThreadSSL();

	Status read(void *buf, int size, Mode mode);
	Status write(const void *buf, int size, Mode mode);

	// "processed" is 1 if the connection was successful
	Status connect(Mode mode);

	/* This does not do anything to the SSL connection. It merely wakes up any
	 * threads waiting on the connection, and tells them to give up; and it
	 * causes any new threads to also give up. This allows for a thread-safe
	 * shutdown which will work even if the peer never sends us a shutdown
	 * reply.
	 *
	 * To actually close the connection, destroy the object.
	 */
	void startShutdown();
};

#endif
