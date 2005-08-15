#ifndef __CLIENTCONNECTION_H__
#define __CLIENTCONNECTION_H__

#include <stdint.h>
#include <openssl/ssl.h>
#include <string>
#include <map>
#include <pthread.h>

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
	std::map<std::string, uint32_t> _props;

	pthread_mutex_t _write_lock;

	bool initiate_ssl();
	bool process_data();
public:
	ClientConnection(int sockfd);
	virtual ~ClientConnection();

	virtual bool process();

	bool sendError(const std::string& message);
	bool reportSuccess();
	bool sendMessageBlock(const MessageBlock *mb);

	uint32_t setProperty(const std::string& prop, uint32_t value);
	uint32_t getProperty(const std::string& prop); // const ... (fails)
	uint32_t delProperty(const std::string& prop);
	
	static bool init();
};

#endif
