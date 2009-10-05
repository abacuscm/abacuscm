/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "servereventregistry.h"
#include "scoped_lock.h"
#include "logger.h"

/*
class ServerEventRegistry {
public:
	typedef void (*EventCallback)(void* cbctx, const void* evctx);

private:
	typedef struct {
		EventCallback func;
		void *ctx;
	} EventListener;

	typedef struct {
		map<uint32_t, EventListener> listeners;
		pthread_mutex_t lock;
	} Event;

	map <std::string, Event> _eventlist;
	pthread_mutex_t _lock;
*/

ServerEventRegistry ServerEventRegistry::_instance;

ServerEventRegistry::ServerEventRegistry()
{
	pthread_mutex_init(&_lock, NULL);
}

ServerEventRegistry::~ServerEventRegistry()
{
	EventList::iterator i;

	for (i = _eventlist.begin(); i != _eventlist.end(); ++i) {
		if (i->second.listeners.size())
			log(LOG_WARNING, "Event '%s' has %lu registered subscribers at shutdown left.", i->first.c_str(), i->second.listeners.size());
		pthread_mutex_destroy(&i->second.lock);
	}

	pthread_mutex_destroy(&_lock);
}

bool ServerEventRegistry::registerEvent(const std::string& eventname)
{
	ScopedLock __(&_lock);

	EventList::iterator i = _eventlist.find(eventname);

	if (i != _eventlist.end()) {
		log(LOG_ERR, "Double Server Event registration for event '%s'.", eventname.c_str());
		return false;
	}

	pthread_mutex_init(&_eventlist[eventname].lock, NULL);
	_eventlist[eventname].next_id = 0;

	return true;
}

uint32_t ServerEventRegistry::subscribeEvent(const std::string& eventname, EventCallback func, void* ctx)
{
	Event *ev = NULL;

	pthread_mutex_lock(&_lock);
	EventList::iterator eli = _eventlist.find(eventname);
	if (eli != _eventlist.end())
		ev = &eli->second;
	pthread_mutex_unlock(&_lock);

	if (!ev) {
		log(LOG_ERR, "Attempt to subscribe to non-existing event '%s'.", eventname.c_str());
		return ~0U;
	}

	EventListener evl = { func, ctx };

	ScopedLock(&ev->lock);

	ev->listeners[++ev->next_id] = evl;
	return ev->next_id;
}

void ServerEventRegistry::unsubscribeEvent(const std::string& eventname, uint32_t subscription_id)
{
	Event *ev = NULL;

	pthread_mutex_lock(&_lock);
	EventList::iterator eli = _eventlist.find(eventname);
	if (eli != _eventlist.end())
		ev = &eli->second;
	pthread_mutex_unlock(&_lock);

	if (!ev) {
		log(LOG_ERR, "Attempt to unsubscribe from non-existing event '%s'.", eventname.c_str());
		return;
	}

	ScopedLock(&ev->lock);
	EventListenerList::iterator s = ev->listeners.find(subscription_id);

	if (s != ev->listeners.end())
		ev->listeners.erase(s);
}

void ServerEventRegistry::triggerEvent(const std::string& eventname, const void* ctx)
{
	Event *ev = NULL;

	pthread_mutex_lock(&_lock);
	EventList::iterator eli = _eventlist.find(eventname);
	if (eli != _eventlist.end())
		ev = &eli->second;
	pthread_mutex_unlock(&_lock);

	if (!ev) {
		log(LOG_ERR, "Attempt to trigger non-existing event '%s'.", eventname.c_str());
		return;
	}

	pthread_mutex_lock(&ev->lock);

	EventListenerList::iterator i;
	for (i = ev->listeners.begin(); i != ev->listeners.end(); ++i)
		i->second.func(i->second.ctx, ctx);
	pthread_mutex_unlock(&ev->lock);
}

ServerEventRegistry& ServerEventRegistry::getInstance()
{
	return _instance;
}
