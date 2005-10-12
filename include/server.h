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

template<class T> class Queue;
class TimedAction;

class Server {
public:
	static uint32_t getId();
	static uint32_t nextUserId();
	static uint32_t nextServerId();
	static uint32_t nextSubmissionId();
	static uint32_t nextClarificationRequestId();
	static uint32_t nextClarificationId();
	static uint32_t nextProblemId();
	static bool hasMessage(uint32_t server_id, uint32_t message_id);
	static void putAck(uint32_t server_id, uint32_t message_id,
			uint32_t ack_server);
	static void setAckQueue(Queue<uint32_t> *ack_queue);
	static bool isContestRunning();
	static uint32_t contestTime();
	static void putTimedAction(TimedAction *action);
	static void setTimedQueue(Queue<TimedAction*> *queue);
};

#endif
