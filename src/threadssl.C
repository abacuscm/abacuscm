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

#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <poll.h>
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <cassert>
#include <cstdlib>
#include "threadssl.h"
#include "logger.h"

using namespace std;

static int n_ssl_locks;
static pthread_mutex_t *ssl_locks;

void ThreadSSL::initialise() {
	n_ssl_locks = CRYPTO_num_locks();
	ssl_locks = (pthread_mutex_t *) malloc(n_ssl_locks * sizeof(pthread_mutex_t));
	if (ssl_locks == NULL) {
		log(LOG_CRIT, "cannot allocate memory for SSL locks");
		exit(1);
	}
	for (int i = 0; i < n_ssl_locks; i++)
		pthread_mutex_init(&ssl_locks[i], NULL);
	CRYPTO_set_locking_callback(ssl_locking_function);
	CRYPTO_set_id_callback(ssl_id_callback);
}

void ThreadSSL::cleanup() {
	CRYPTO_set_id_callback(NULL);
	CRYPTO_set_locking_callback(NULL);
	for (int i = 0; i < n_ssl_locks; i++)
		pthread_mutex_destroy(&ssl_locks[i]);
	free(ssl_locks);
}

ThreadSSL::ThreadSSL(SSL *ssl) : _ssl(ssl), _shutting_down(false) {
	int shutdown_notify[2];

	if (pipe(shutdown_notify) == -1) {
		lerror("pipe");
		shutdown_notify[0] = shutdown_notify[1] = -1;
	}

	_shutdown_notify_read = shutdown_notify[0];
	_shutdown_notify_write = shutdown_notify[1];
	pthread_mutex_init(&_lock, NULL);
}

bool ThreadSSL::postProcess(Status &stat, int fd, int len, Mode mode) {
	struct pollfd pfds[2];

	assert(fd >= 0);
	pfds[0].fd = fd;
	pfds[0].events = 0;
	pfds[1].fd = _shutdown_notify_read;
	pfds[1].events = POLLIN;

	switch (stat.err) {
	case SSL_ERROR_NONE:
		if (mode == BLOCK_PARTIAL || stat.processed == len)
			return true;    // all done
		else
			return false;    // need to go around again
		break;
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_ACCEPT:
		pfds[0].events |= POLLIN;
		break;
	case SSL_ERROR_WANT_WRITE:
	case SSL_ERROR_WANT_CONNECT:
		pfds[0].events |= POLLOUT;
		break;
	default:
		return true;    // pass error up to caller
	}

	/* We only get here if we need to poll. The timeout is just a
	 * belt-and-braces to make sure that if we have a race condition in which
	 * we miss the data arriving, we get another chance to go around again.
	 */
	while (poll(pfds, _shutdown_notify_read != -1 ? 2 : 1, 10000) < 0) {
		if (errno != EINTR) {
			stat.err = SSL_ERROR_SYSCALL;
			return true;
		}
	}
	return false;
}

ThreadSSL::Status ThreadSSL::read(void *buf, int len, Mode mode) {
	char *buffer = (char *) buf;
	Status stat;
	int ret;
	int fd;

	stat.processed = 0;
	while (true) {
		pthread_mutex_lock(&_lock);
		if (_shutting_down) {
			stat.err = SSL_ERROR_ZERO_RETURN;
			pthread_mutex_unlock(&_lock);
			return stat;
		}
		ret = SSL_read(_ssl, buffer + stat.processed, len - stat.processed);
		if (ret == 0) {
			/* We don't particularly care about the difference between
			 * clean and unclean shutdown, so claim it was clean.
			 */
			stat.err = SSL_ERROR_ZERO_RETURN;
		}
		else
			stat.err = SSL_get_error(_ssl, ret);
		fd = SSL_get_fd(_ssl);
		pthread_mutex_unlock(&_lock);

		if (ret > 0)
			stat.processed += ret;
		if (mode == NONBLOCK)
			return stat;
		if (postProcess(stat, fd, len, mode))
			return stat;
	}
}

ThreadSSL::Status ThreadSSL::write(const void *buf, int len, Mode mode)
{
	const char *buffer = (const char *) buf;
	Status stat;
	int ret;
	int fd;

	stat.processed = 0;
	while (true) {
		pthread_mutex_lock(&_lock);
		if (_shutting_down) {
			// shutdown has already been called on our side
			stat.err = SSL_ERROR_ZERO_RETURN;
			pthread_mutex_unlock(&_lock);
			return stat;
		}
		ret = SSL_write(_ssl, buffer + stat.processed, len - stat.processed);
		if (ret == 0) {
			/* We don't particularly care about the difference between
			 * clean and unclean shutdown, so claim it was clean.
			 */
			stat.err = SSL_ERROR_ZERO_RETURN;
		}
		else
			stat.err = SSL_get_error(_ssl, ret);
		fd = SSL_get_fd(_ssl);
		pthread_mutex_unlock(&_lock);

		if (ret > 0)
			stat.processed += ret;
		if (mode == NONBLOCK)
			return stat;
		if (postProcess(stat, fd, len, mode))
			return stat;
	}
}

ThreadSSL::Status ThreadSSL::connect(Mode mode) {
	Status stat;
	int ret;
	int fd;

	stat.processed = 0;
	while (true) {
		pthread_mutex_lock(&_lock);
		if (_shutting_down) {
			// shutdown has already been called on our side
			stat.err = SSL_ERROR_ZERO_RETURN;
			pthread_mutex_unlock(&_lock);
			return stat;
		}

		ret = SSL_connect(_ssl);
		stat.err = SSL_get_error(_ssl, ret);
		fd = SSL_get_fd(_ssl);
		pthread_mutex_unlock(&_lock);

		if (ret > 0)
			stat.processed += ret;
		if (mode == NONBLOCK)
			return stat;
		if (postProcess(stat, fd, 1, mode))
			return stat;
	}
}

void ThreadSSL::startShutdown() {
	pthread_mutex_lock(&_lock);
	if (_shutting_down) {
		pthread_mutex_unlock(&_lock);
		return;
	}
	_shutting_down = true;
	pthread_mutex_unlock(&_lock);

	char dummy[1] = {0};
	/* Wake up other threads that are polling */
	::write(_shutdown_notify_write, dummy, 1);
}

ThreadSSL::~ThreadSSL() {
	close(_shutdown_notify_read);
	close(_shutdown_notify_write);
	SSL_shutdown(_ssl);
	SSL_free(_ssl);
	pthread_mutex_destroy(&_lock);
}
