#ifndef __CLIENTCONNECTION_H__
#define __CLIENTCONNECTION_H__

#include <openssl/ssl.h>
#include <string>

#include "socket.h"

#define USER_TYPE_NONE			0
#define USER_TYPE_ADMIN			1
#define USER_TYPE_JUDGE			2
#define USER_TYPE_PARTICIPANT	3
#define USER_TYPE_MARKER		4

#define CLIENT_BFR_SIZE			512

class MessageBlock;

class ClientConnection : public Socket {
private:
	static SSL_METHOD *_method;
	static SSL_CTX *_context;

	SSL *_ssl;

	MessageBlock *_message;

	bool initiate_ssl();
	bool process_data();
public:
	ClientConnection(int sockfd);
	virtual ~ClientConnection();

	virtual bool process();

	int getUserType();
	void sendError(const std::string& message);
	void reportSuccess();
	void sendMessageBlock(MessageBlock *mb);

	static bool init();
};

#endif
