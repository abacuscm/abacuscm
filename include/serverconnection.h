#ifndef __SERVERCONNECTION_H__
#define __SERVERCONNECTION_H__

#include <string>
#include <openssl/ssl.h>

class MessageBlock;

typedef void (*EventCallback)(const MessageBlock*, void *);

class ServerConnection {
private:
	int _sock;
	SSL *_ssl;
public:
	ServerConnection();
	~ServerConnection();

	bool connect(std::string servername, int port);
	bool disconnect();

	bool auth(std::string username, std::string password);

	bool registerEventCallback(std::string event, EventCallback func, void *custom);
	bool deregisterEventCallback(std::string event, EventCallback func);
};

#endif
