/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __SERVEREVENTREGISTRY_H__
#define __SERVEREVENTREGISTRY_H__

#include <stdint.h>
#include <pthread.h>
#include <map>
#include <string>

class ServerEventRegistry {
public:
	typedef void (*EventCallback)(void* cbctx, const void* evctx);

private:
	typedef struct {
		EventCallback func;
		void *ctx;
	} EventListener;

	typedef std::map<uint32_t, EventListener> EventListenerList;

	typedef struct {
		EventListenerList listeners;
		uint32_t next_id;
		pthread_mutex_t lock;
	} Event;

	typedef std::map <std::string, Event> EventList;

	EventList _eventlist;
	pthread_mutex_t _lock;

	ServerEventRegistry();
	~ServerEventRegistry();

	static ServerEventRegistry _instance;
public:
	bool registerEvent(const std::string& eventname);
	uint32_t subscribeEvent(const std::string& eventname, EventCallback func, void* ctx);
	void unsubscribeEvent(const std::string& eventname, uint32_t subscription_id);
	void triggerEvent(const std::string& eventname, const void* ctx);

	static ServerEventRegistry& getInstance();
};

#endif
