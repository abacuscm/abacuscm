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
	
	int _sock;
	SSL *_ssl;
	SSL_CTX *_ctx;
	pthread_mutex_t _send_lock;
	Queue<MessageBlock *> _responses;
	std::map<std::string, std::list<CallbackData> > _eventmap;
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
