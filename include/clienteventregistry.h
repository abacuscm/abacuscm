/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __CLIENTEVENTREGISTRY_H__
#define __CLIENTEVENTREGISTRY_H__

#include <stdint.h>
#include <pthread.h>
#include <set>
#include <map>

class ClientConnection;
class MessageBlock;

class ClientEventRegistry {
private:
	class Event {
	private:
		typedef std::set<ClientConnection*> ClientConnectionPool;

		ClientConnectionPool _clients;
		ClientConnection* _next_single;
		pthread_mutex_t _lock;
	public:
		Event();
		~Event();

		void triggerEvent(const MessageBlock* mb);
		void triggerOne(const MessageBlock*mb, bool remove);
		void registerClient(ClientConnection *);
		void deregisterClient(ClientConnection *);
		bool isClientRegistered(ClientConnection *);
	};

	typedef std::map<std::string, Event*> EventMap;
	typedef std::map<uint32_t, ClientConnection*> ClientMap;

	EventMap _eventmap;
	ClientMap _clients;
	pthread_mutex_t _lock;

	ClientEventRegistry();
	~ClientEventRegistry();

	static ClientEventRegistry _instance;
public:
	void registerClient(ClientConnection *cc);
	bool registerClient(std::string eventname, ClientConnection *cc);
	void deregisterClient(ClientConnection *cc);
	void deregisterClient(std::string eventname, ClientConnection *cc);
	bool isClientRegistered(std::string eventname, ClientConnection *cc);

	void registerEvent(std::string eventname);
	void triggerEvent(std::string eventname, const MessageBlock *mb);
	void broadcastEvent(const MessageBlock *mb); /* To ALL clients */
//	void triggerOne(std::string eventname, const MessageBlock*mb, bool remove);
	void sendMessage(uint32_t user_id, const MessageBlock *mb);

	static ClientEventRegistry& getInstance();
};

#endif
