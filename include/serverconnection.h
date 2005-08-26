#ifndef __SERVERCONNECTION_H__
#define __SERVERCONNECTION_H__

#include "queue.h"

#include <string>
#include <list>
#include <map>
#include <openssl/ssl.h>

class MessageBlock;

typedef void (*EventCallback)(const MessageBlock*, void *);

class ServerConnection {
private:
	struct CallbackData {
		EventCallback func;
		void *p;
	};
	typedef std::list<CallbackData> CallbackList;
	typedef std::map<std::string, CallbackList> EventMap;

	pthread_mutex_t _lock_sockdata;
	pthread_mutex_t _lock_sender;
	pthread_cond_t _cond_read_count;

	int _reader_count; // -1 indicates that a write lock is in effect.	
	int _sock;
	SSL *_ssl;
	SSL_CTX *_ctx;

	pthread_mutex_t _lock_response;
	pthread_cond_t _cond_response;
	MessageBlock* _response;

	pthread_mutex_t _lock_eventmap;
	EventMap _eventmap;

	pthread_mutex_t _lock_thread;
	pthread_cond_t _cond_thread;
	bool _has_thread;

	void ensureThread();
	void killThread();

	void sockdata_readlock();
	void sockdata_writelock();
	void sockdata_unlock();

	void processMB(MessageBlock *mb);
	MessageBlock *sendMB(MessageBlock *mb);
	void* receive_thread();

	static void* thread_spawner(void*);
public:
	ServerConnection();
	~ServerConnection();

	bool connect(std::string servername, std::string service);
	bool disconnect();

	bool auth(std::string username, std::string password);

	bool registerEventCallback(std::string event, EventCallback func, void *custom);
	bool deregisterEventCallback(std::string event, EventCallback func);
};

#endif
