#include "serverconnection.h"
#include "logger.h"
#include "config.h"
#include "messageblock.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <openssl/rand.h>
#include <sstream>
#include <string.h>

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
	if(_ctx)
		SSL_CTX_free(_ctx);
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

	sockdata_unlock();
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

bool ServerConnection::simpleAction(MessageBlock &mb) {
	MessageBlock *ret = sendMB(&mb);
	if(!ret)
		return false;

	bool response = ret->action() == "ok";
	if(!response)
		log(LOG_ERR, "%s", (*ret)["msg"].c_str());

	delete ret;
	
	return response;
}

vector<string> ServerConnection::vectorAction(MessageBlock &mb, string prefix) {
	MessageBlock *ret = sendMB(&mb);
	if(!ret)
		return vector<string>();

	vector<string> resp(vectorFromMB(*ret, prefix));
	delete ret;
	return resp;
}

string ServerConnection::stringAction(MessageBlock &mb, string fieldname) {
	MessageBlock *ret = sendMB(&mb);
	if(!ret)
		return "";

	bool response = ret->action() == "ok";
	string result;
	if(!response)
		log(LOG_ERR, "%s", (*ret)["msg"].c_str());
	else
		result = (*ret)[fieldname];

	delete ret;

	return result;
}

vector<string> ServerConnection::vectorFromMB(MessageBlock &mb, string prefix) {
	unsigned i = 0;
	ostringstream nattr;
	nattr << prefix << i;
	vector<string> result;
	while(mb.hasAttribute(nattr.str())) {
		result.push_back(mb[nattr.str()]);
		nattr.str("");
		nattr << prefix << ++i;
	}
	return result;
}

bool ServerConnection::auth(string username, string password) {
	MessageBlock mb("auth");

	mb["user"] = username;
	mb["pass"] = password;

	return simpleAction(mb);
}

string ServerConnection::whatAmI() {
	MessageBlock mb("whatami");

	return stringAction(mb, "type");
}

bool ServerConnection::createuser(string username, string password, string type) {
	MessageBlock mb("adduser");

	mb["username"] = username;
	mb["passwd"] = password;
	mb["type"] = type;

	return simpleAction(mb);
}

vector<string> ServerConnection::getProblemTypes() {
	MessageBlock mb("getprobtypes");

	return vectorAction(mb, "type");
}

vector<string> ServerConnection::getServerList() {
	MessageBlock mb("getserverlist");

	return vectorAction(mb, "server");
}

string ServerConnection::getProblemDescription(string type) {
	MessageBlock mb("getprobdescript");
	mb["type"] = type;

	return stringAction(mb, "descript");
}

bool ServerConnection::setProblemAttributes(uint32_t prob_id, std::string type,
			const AttributeMap& normal, const AttributeMap& file) {
	ostringstream ostrstrm;
	AttributeMap::const_iterator i;
	MessageBlock mb("setprobattrs");
	list<int> fds;
	uint32_t file_offset = 0;

	ostrstrm << prob_id;
	
	mb["prob_id"] = ostrstrm.str();
	mb["prob_type"] = type;
	
	for(i = file.begin(); i != file.end(); ++i) {
		int fd = open(i->second.c_str(), O_RDONLY | O_EXCL);
		if(fd < 0) {
			lerror(("open(" + i->second + ")").c_str());
			for(list<int>::iterator j = fds.begin(); j != fds.end(); ++j)
				close(*j);

			return false;
		}
		struct stat st_stat;
		if(fstat(fd, &st_stat) < 0) {
			lerror(("fstat(" + i->second + ")").c_str());
			for(list<int>::iterator j = fds.begin(); j != fds.end(); ++j)
				close(*j);

			return false;
		}
		
		ostrstrm.str("");
		char *tmp = strdup(i->second.c_str());
		ostrstrm << file_offset << " " << st_stat.st_size << " " << basename(tmp);
		free(tmp);

		mb[i->first] = ostrstrm.str();
		file_offset += st_stat.st_size;
		fds.push_back(fd);
	}

	log(LOG_DEBUG, "Loading %u bytes ...", file_offset);
	char *filedata = new char[file_offset];
	char *pos = filedata;
	for(list<int>::iterator fd_ptr = fds.begin(); fd_ptr != fds.end(); ++fd_ptr) {
		struct stat st_stat;
		if(fstat(*fd_ptr, &st_stat) < 0) {
			lerror(("fstat(" + i->second + ")").c_str());
			for( ; fd_ptr != fds.end(); ++fd_ptr)
				close(*fd_ptr);
			delete []filedata;
			return false;
		}
		while(st_stat.st_size) {
			int res = read(*fd_ptr, pos, st_stat.st_size);
			log(LOG_DEBUG, "read returned %d", res);
			if(res < 0) {
				lerror("read");
				for( ; fd_ptr != fds.end(); ++fd_ptr)
					close(*fd_ptr);
				delete []filedata;
				return false;
			}
			st_stat.st_size -= res;
			pos += res;
		}
	}
	if(filedata + file_offset != pos)
		log(LOG_ERR, "Potential problem, the amount of data read doesn't match up with the calculated amount of data required (Loaded %ld, expected %u)", (long)(pos - filedata), file_offset);
	mb.setContent(filedata, file_offset);
	delete []filedata;

	for(i = normal.begin(); i != normal.end(); ++i) {
		mb[i->first] = i->second;
	}

	mb.dump();

	return simpleAction(mb);
}

bool ServerConnection::registerEventCallback(string event, EventCallback func, void *custom) {
	pthread_mutex_lock(&_lock_eventmap);

	CallbackList &list = _eventmap[event];
	CallbackList::iterator i;
	for(i = list.begin(); i != list.end(); ++i) {
		if(i->func == func)
			break;
	}
	i->func = func;
	i->p = custom;
	
	pthread_mutex_unlock(&_lock_eventmap);
	return true;
}

bool ServerConnection::deregisterEventCallback(string event, EventCallback func) {
	pthread_mutex_lock(&_lock_eventmap);

	CallbackList &list = _eventmap[event];
	CallbackList::iterator i;
	for(i = list.begin(); i != list.end(); ++i) {
		if(i->func == func)
			list.erase(i);
	}
	
	pthread_mutex_unlock(&_lock_eventmap);
	return true;
}

/**
 * BUG: RACE CONDITION:
 * We actually need to lock the decisions to terminate the thread
 * with _lock_thread, not the actual shutdown.  If the thread
 * decides to shut down then gets interrupted, a new thread starts
 * decides to not start (cause another thread is running) and then
 * the orriginal thread runs again then both threads will die.
 *
 * This _should_ not be a problem since we start threads when we
 * establish connections, and it takes longer to re-establish a
 * connection that what it should take for this thread to die.  But
 * if that condition _always_ holds then testing for a thread is
 * irrelavent.
 */
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
