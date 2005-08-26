#include "serverconnection.h"
#include "logger.h"
#include "config.h"
#include "messageblock.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/rand.h>

// 16K is the max size of an SSL block, thus
// optimal in terms of speed, but memory intensive.
#define BFR_SIZE		(16 * 1024)

using namespace std;

ServerConnection::ServerConnection() {
	_reader_count = 0;
	_sock = -1;
	_ssl = NULL;
	_ctx = NULL;
	_response = NULL;
	_has_thread = false;

	pthread_mutex_init(&_lock_sockdata, NULL);
	pthread_mutex_init(&_lock_sender, NULL);
	pthread_mutex_init(&_lock_response, NULL);
	pthread_mutex_init(&_lock_eventmap, NULL);
	pthread_mutex_init(&_lock_thread, NULL);

	pthread_cond_init(&_cond_read_count, NULL);
	pthread_cond_init(&_cond_response, NULL);
	pthread_cond_init(&_cond_thread, NULL);
}

ServerConnection::~ServerConnection() {
	if(_sock >= 0)
		disconnect();
}

void ServerConnection::sockdata_readlock() {
	pthread_mutex_lock(&_lock_sockdata);
	while(_reader_count < 0)
		pthread_cond_wait(&_cond_read_count, &_lock_sockdata);
	_reader_count++;
	pthread_mutex_unlock(&_lock_sockdata);
}

void ServerConnection::sockdata_writelock() {
	pthread_mutex_lock(&_lock_sockdata);
	while(_reader_count != 0)
		pthread_cond_wait(&_cond_read_count, &_lock_sockdata);
	_reader_count = -1;
	pthread_mutex_unlock(&_lock_sockdata);
}

void ServerConnection::sockdata_unlock() {
	pthread_mutex_lock(&_lock_sockdata);
	if(_reader_count < 0) {
		_reader_count = 0;
		// there can be many readers/writers waiting,
		// wake them all up so that if a reader grabs
		// the lock all readers can grab.
		pthread_cond_broadcast(&_cond_read_count);
	} else {
		// there can only be writers waiting so only
		// wake one up.
		if(--_reader_count == 0)
			pthread_cond_signal(&_cond_read_count);
	}
	pthread_mutex_unlock(&_lock_sockdata);	
}

bool ServerConnection::connect(string server, string service) {
	Config &config = Config::getConfig();
	
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	addrinfo * adinf = NULL;
	addrinfo * i;

	sockdata_writelock();

	if(_sock > 0) {
		log(LOG_ERR, "Already connected!");
		goto err;
	}

	if(!_ctx) {
		_ctx = SSL_CTX_new(TLSv1_client_method());
		if(!_ctx) {
			log_ssl_errors("SSL_CTX_new");
			goto err;
		}

		if(!SSL_CTX_load_verify_locations(_ctx,
					config["server"]["cacert"].c_str(), NULL)) {
			log_ssl_errors("SSL_CTX_load_verify_locations");
			SSL_CTX_free(_ctx);
			_ctx = NULL;
			goto err;
		}

		SSL_CTX_set_verify(_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
		SSL_CTX_set_mode(_ctx, SSL_MODE_AUTO_RETRY);
	}

	if(getaddrinfo(server.c_str(), service.c_str(), &hints, &adinf) < 0) {
		lerror("getaddrinfo");
		goto err;
	}

	for(i = adinf; i; i = i->ai_next) {
		_sock = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
		if(_sock < 0)
			lerror("socket");
		else // yes, this can go in the condition above but then we lose i.
			break;
	}

	if(_sock < 0)
		goto err;
	
	if(::connect(_sock, i->ai_addr, i->ai_addrlen) < 0) {
		lerror("connect");
		goto err;
	}
	freeaddrinfo(adinf);
	adinf = NULL;

	_ssl = SSL_new(_ctx);
	if(!_ssl) {
		log_ssl_errors("SSL_new");
		goto err;
	}

	if(!SSL_set_fd(_ssl, _sock)) {
		log_ssl_errors("SSL_set_fd");
		goto err;
	}

	if(SSL_connect(_ssl) < 1) {
		log_ssl_errors("SSL_connect");
		goto err;
	}

	log(LOG_DEBUG, "TODO:  Check servername against cn of certificate");

	ensureThread();
	log(LOG_DEBUG, "asdf");

	sockdata_unlock();
	log(LOG_DEBUG, "asdf");
	return true;
err:
	if(_ssl) {
		SSL_free(_ssl);
		_ssl = NULL;
	}
	if(_sock) {
		close(_sock);
		_sock = -1;
	}
	if(adinf)
		freeaddrinfo(adinf);
	sockdata_unlock();
	return false;
}

bool ServerConnection::disconnect() {
	// This seems to break the rules but I'm
	// not modifying the value of _ssl directly,
	// I'm changing an underlying state that will
	// further use of _ssl to fail, and cause all
	// threads using it to drop out and allow me
	// to obtain a writelock instead.
	sockdata_readlock();
	if(_ssl)
		SSL_shutdown(_ssl);
	sockdata_unlock();
	
	sockdata_writelock();
	if(_sock < 0) {
		log(LOG_WARNING, "Attempting to disconnect non-connected socket.");
		sockdata_unlock();
		return false;
	}
	if(_ssl) {
		SSL_free(_ssl);
		_ssl = NULL;
	}
	close(_sock);
	_sock = -1;
	sockdata_unlock();
	return true;
}

void ServerConnection::processMB(MessageBlock *mb) {
	if(mb->action() == "ok" || mb->action() == "err") {
		pthread_mutex_lock(&_lock_response);
		if(_response)
			log(LOG_WARNING, "Losing result - this could potentially cause problems");
		_response = mb;
		pthread_cond_signal(&_cond_response);
		pthread_mutex_unlock(&_lock_response);
	} else {
		NOT_IMPLEMENTED();
		delete mb;
	}
}

MessageBlock* ServerConnection::sendMB(MessageBlock *mb) {
	MessageBlock *res = NULL;
	sockdata_readlock();

	if(!_ssl) {
		log(LOG_ERR, "Attempting to send data to a NULL _ssl socket");
		sockdata_unlock();
		return NULL;
	}

	pthread_mutex_lock(&_lock_sender);
	if(mb->writeToSSL(_ssl)) {
		pthread_mutex_unlock(&_lock_sender);
		pthread_mutex_lock(&_lock_response);
		while(!_response)
			pthread_cond_wait(&_cond_response, &_lock_response);
		res = _response;
		_response = NULL;
		pthread_mutex_unlock(&_lock_response);
	} else
		pthread_mutex_unlock(&_lock_sender);

	sockdata_unlock();
	return res;
}

bool ServerConnection::auth(string username, string password) {
	MessageBlock mb("auth");

	mb["user"] = username;
	mb["pass"] = password;

	MessageBlock *ret = sendMB(&mb);
	if(!ret)
		return false;

	bool response = ret->action() == "ok";
	if(!response)
		log(LOG_ERR, "%s", (*ret)["msg"].c_str());

	delete ret;
	
	return response;
}

bool ServerConnection::registerEventCallback(string event, EventCallback func, void *custom) {
	NOT_IMPLEMENTED();
	return false;
}

bool ServerConnection::deregisterEventCallback(string event, EventCallback func) {
	NOT_IMPLEMENTED();
	return false;
}

void* ServerConnection::receive_thread() {
	char buffer[BFR_SIZE];
	MessageBlock *mb = NULL;

	pthread_mutex_lock(&_lock_thread);
	if(_has_thread) {
		pthread_mutex_unlock(&_lock_thread);
		log(LOG_WARNING, "Double spawned ServerConnection::receive_thread");
		return NULL;
	}
	_has_thread = true;
	pthread_mutex_unlock(&_lock_thread);

	log(LOG_DEBUG, "Starting ServerConnection receive_thread");
	while(true) {
		sockdata_readlock();
		if(!_ssl) {
			sockdata_unlock();
			log(LOG_CRIT, "This is bad!  Null _ssl pointer!");
			break;
		}
		int res = SSL_read(_ssl, buffer, BFR_SIZE);
		sockdata_unlock();
	
		char *pos = buffer;
		if(res < 0) {
			log_ssl_errors("SSL_read");
		} else if(res == 0) {
			log(LOG_INFO, "Connection got shut down, terminating receive_thread");
			break;
		} else while(res > 0) {
			if(!mb)
				mb = new MessageBlock();
			int bytes_used = mb->addBytes(pos, res);

			if(bytes_used < 0) {
				log(LOG_ERR, "Sync error!  This is extremely serious and probably indicates a bug!");
				delete mb;
				mb = NULL;
			} else if(bytes_used == 0) {
				res = 0;
			} else {
				// processMB either deletes mb or puts it in a queue.
				processMB(mb);
				mb = NULL;
				res -= bytes_used;
				pos += bytes_used;
			}
		}
	}
	
	pthread_mutex_lock(&_lock_thread);
	_has_thread = false;
	pthread_mutex_unlock(&_lock_thread);
	
	log(LOG_DEBUG, "Stopping ServerConnection receive_thread");

	return NULL;
}

void* ServerConnection::thread_spawner(void *p) {
	return ((ServerConnection*)p)->receive_thread();
}

void ServerConnection::ensureThread() {
	pthread_mutex_lock(&_lock_thread);
	if(!_has_thread) {
		pthread_t tid;
		pthread_create(&tid, NULL, thread_spawner, this);
		pthread_detach(tid);
	}
	pthread_mutex_unlock(&_lock_thread);
}

static void init() __attribute__((constructor));
static void init() {
	SSL_library_init();
	SSL_load_error_strings();
	RAND_load_file("/dev/urandom", 4096);
}
