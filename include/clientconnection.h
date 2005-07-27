#ifndef __CLIENTCONNECTION_H__
#define __CLIENTCONNECTION_H__

#include <openssl/ssl.h>

#include "socket.h"

#define USER_TYPE_NONE			(-1)
#define USER_TYPE_ADMIN			1
#define USER_TYPE_JUDGE			2
#define USER_TYPE_PARTICIPANT	3
#define USER_TYPE_MARKER		4

class ClientConnection : public Socket {
private:
	static SSL_METHOD *_method;
	static SSL_CTX *_context;

	SSL *_ssl;

	bool initiate_ssl();
public:
	ClientConnection(int sockfd);
	virtual ~ClientConnection();

	virtual bool process();

	int getUserType();

	static bool init();
};

#endif
