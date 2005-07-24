#ifndef __CLIENTLISTENER_H__
#define __CLIENTLISTENER_H__

#include "socket.h"

#include <set>

typedef std::set<Socket*> SocketPool;

class ClientListener : public Socket {
private:
	SocketPool* _pool;
public:
	ClientListener();
	virtual ~ClientListener();

	bool init(SocketPool *pool);

	virtual bool process();
};

#endif
