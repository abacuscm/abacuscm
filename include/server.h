/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __SERVER_H__
#define __SERVER_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdint.h>
#include <string>

// The maximum number of servers which can be allocated.
#define MAX_SERVERS		16

// This must be a power of 2 >= MAX_SERVERS
#define ID_GRANULARITY	MAX_SERVERS

template<class T> class Queue;
class TimedAction;
class SocketPool;
class Socket;

class Server {
public:
	static uint32_t server_id(const std::string& name);
	static std::string servername(uint32_t user_id);

	static uint32_t getId();
	static uint32_t nextServerId();
	static uint32_t nextSubmissionId();
	static uint32_t nextClarificationRequestId();
	static uint32_t nextClarificationId();
	static uint32_t nextProblemId();
	static bool hasMessage(uint32_t server_id, uint32_t message_id);
	static void putAck(uint32_t server_id, uint32_t message_id,
			uint32_t ack_server);
	static void setAckQueue(Queue<uint32_t> *ack_queue);
	static void putTimedAction(TimedAction *action);
	static void setTimedQueue(Queue<TimedAction*> *queue);
	static void setSocketQueue(Queue<Socket*> *queue);
	static void setSocketPool(SocketPool *pool);
	static void putSocket(Socket*, bool queue_immediate = false);
};

#endif
