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
#include <string>
#include "misc.h"
#include "permissions.h"

class ClientConnection;
class MessageBlock;

class ClientEventRegistry {
private:
	class Event {
	private:
		typedef std::set<ClientConnection*> ClientConnectionPool;

		ClientConnectionPool _clients;
		ClientConnection* _next_single;
		PermissionTest _pt;
		pthread_mutex_t _lock;
	public:
		explicit Event(const PermissionTest &pt);
		~Event();

		void triggerEvent(const MessageBlock* mb);
		void triggerGroupEvent(const MessageBlock* mb, uint32_t group_id);
		void triggerOne(const MessageBlock*mb, bool remove);

		// registerClient and deregisterClient return true on success, and
		// false if the client is not permitted to register for this event.
		bool registerClient(ClientConnection *);
		bool deregisterClient(ClientConnection *);
		bool isClientRegistered(ClientConnection *);
	};

	typedef std::map<std::string, Event*> EventMap;
	typedef std::multimap<uint32_t, ClientConnection*> ClientMap;

	EventMap _eventmap;
	ClientMap _clients;
	pthread_mutex_t _lock;

	ClientEventRegistry();
	~ClientEventRegistry();

	static ClientEventRegistry _instance;
public:
	void registerClient(ClientConnection *cc);
	bool registerClient(const std::string &eventname, ClientConnection *cc);
	void deregisterClient(ClientConnection *cc);
	bool deregisterClient(const std::string &eventname, ClientConnection *cc);
	bool isClientRegistered(const std::string &eventname, ClientConnection *cc);

	// Creates an event, and determines which users should be allowed to
	// subscribe to it.
	void registerEvent(const std::string &eventname, const PermissionTest &pt);

	// Send message to clients registered via registerClient for the event
	void triggerEvent(const std::string &eventname, const MessageBlock *mb);

	// Like triggerEvent, but only for one group
	void triggerGroupEvent(const std::string &eventname, const MessageBlock *mb, uint32_t group_id);

	// Send message to all clients either with the given permissions, or with the
	// given user id. If user_id is zero it is ignored. Note that clients do
	// not need to register with the event to receive it.
	void broadcastEvent(uint32_t user_id, const PermissionTest &pt, const MessageBlock *mb);

	// Send message just to one user
	void sendMessage(uint32_t user_id, const MessageBlock *mb);

	static ClientEventRegistry& getInstance();
};

#endif
