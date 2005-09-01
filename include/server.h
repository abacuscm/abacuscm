#ifndef __SERVER_H__
#define __SERVER_H__

#include <stdint.h>

#define USER_NONE		0
#define USER_ADMIN		1
#define USER_JUDGE		2
#define USER_CONTESTANT	3

// The maximum number of servers which can be allocated.
#define MAX_SERVERS		16

// This must be a power of 2 >= MAX_SERVERS
#define ID_GRANULARITY	MAX_SERVERS

class Server {
public:
	static uint32_t getId();
	static uint32_t nextUserId();
	static uint32_t nextServerId();
	static bool hasMessage(uint32_t server_id, uint32_t message_id);
	static void flushMessages(uint32_t server_id);
};

#endif
