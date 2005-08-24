#ifndef __SERVERCONNECTION_H__
#define __SERVERCONNECTION_H__

#include <string>

class MessageBlock;

typedef void (*EventCallback)(const MessageBlock*, void *);

class ServerConnection {
private:
public:
	ServerConnection();
	~ServerConnection();

	bool connect(std::string servername, int port);
	bool disconnect();

	bool registerEventCallback(std::string event, EventCallback func, void *custom);
	bool deregisterEventCallback(std::string event, EventCallback func);
};

#endif
