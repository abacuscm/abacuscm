/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "eventregister.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "logger.h"

using namespace std;

EventRegister EventRegister::_instance;

EventRegister::EventRegister() {
    pthread_mutex_init(&_lock, NULL);
    _clients.clear();
}

EventRegister::~EventRegister() {
	pthread_mutex_destroy(&_lock);
	EventMap::iterator i;
	for(i = _eventmap.begin(); i != _eventmap.end(); ++i)
		delete i->second;
}

void EventRegister::registerClient(ClientConnection *cc) {
    if (cc == NULL) {
        log(LOG_ERR, "F**K!!!! A NULL ClientConnection!!!");
        return;
    }
	uint32_t user_id = cc->getProperty("user_id");
	pthread_mutex_lock(&_lock);
	_clients[user_id] = cc;
	pthread_mutex_unlock(&_lock);
};

bool EventRegister::registerClient(string eventname, ClientConnection *cc) {
	EventMap::iterator i = _eventmap.find(eventname);
    if(i == _eventmap.end()) {
		log(LOG_INFO, "Attempt to register for non-existant event '%s' from user %u.", eventname.c_str(), cc->getProperty("user_id"));
		return false;
	}
	i->second->registerClient(cc);
	return true;
}

void EventRegister::deregisterClient(ClientConnection *cc) {
	uint32_t user_id = cc->getProperty("user_id");
	pthread_mutex_lock(&_lock);

	EventMap::iterator e;
	for(e = _eventmap.begin(); e != _eventmap.end(); ++e)
		e->second->deregisterClient(cc);

	ClientMap::iterator c = _clients.find(user_id);
	if(c != _clients.end())
		_clients.erase(c);
	pthread_mutex_unlock(&_lock);
}

void EventRegister::deregisterClient(string eventname, ClientConnection *cc) {
	Event* ev = _eventmap[eventname];
	log(LOG_DEBUG, "Deregistering from event '%s'", eventname.c_str());
	if(!ev)
		log(LOG_INFO, "Attempt by user %d to deregister from non-existant event '%s'", cc->getProperty("user_id"), eventname.c_str());
	else
		ev->deregisterClient(cc);
}

bool EventRegister::isClientRegistered(string eventname, ClientConnection *cc) {
	EventMap::iterator i = _eventmap.find(eventname);
    if(i == _eventmap.end()) {
	log(LOG_INFO, "Attempt by user %d to check registration for non-existant event '%s'", cc->getProperty("user_id"), eventname.c_str());
	return false;
    }
	else
		return i->second->isClientRegistered(cc);
}

void EventRegister::registerEvent(string eventname) {
	// Yes, I know here is a race condition, but most (if not all)
	// events will only register once at startup whilst we aren't
	// multi-threading yet.
	Event* ev = _eventmap[eventname];
	if(ev)
		log(LOG_NOTICE, "Attempt to re-register event '%s'", eventname.c_str());
	else
		_eventmap[eventname] = new Event();
}

void EventRegister::triggerEvent(string eventname, const MessageBlock *mb) {
	EventMap::iterator i = _eventmap.find(eventname);
	if(i == _eventmap.end())
		log(LOG_ERR, "Attempt to trigger unknown event '%s'",
				eventname.c_str());
	else
		i->second->triggerEvent(mb);
}

void EventRegister::broadcastEvent(const MessageBlock *mb) {
	pthread_mutex_lock(&_lock);
    for(ClientMap::iterator i = _clients.begin(); i != _clients.end(); ++i) {
        log(LOG_INFO, "broadcasting to client %p (with id %d)", i->second, i->first);
        if (i->second != NULL)
            i->second->sendMessageBlock(mb);
    }
	pthread_mutex_unlock(&_lock);
}

void EventRegister::sendMessage(uint32_t user_id, const MessageBlock *mb) {
    pthread_mutex_lock(&_lock);
    ClientMap::iterator i = _clients.find(user_id);
    if (i != _clients.end()) {
        ClientConnection *cc = i->second;
        if(cc)
            cc->sendMessageBlock(mb);
    }
	pthread_mutex_unlock(&_lock);
}

EventRegister& EventRegister::getInstance() {
	return _instance;
}

EventRegister::Event::Event() {
	pthread_mutex_init(&_lock, NULL);
}

EventRegister::Event::~Event() {
	pthread_mutex_destroy(&_lock);
}

void EventRegister::Event::triggerEvent(const MessageBlock *mb) {
	pthread_mutex_lock(&_lock);
	ClientConnectionPool::iterator i;
	for(i = _clients.begin(); i != _clients.end(); ++i)
		(*i)->sendMessageBlock(mb);
	pthread_mutex_unlock(&_lock);
}

void EventRegister::Event::registerClient(ClientConnection *cc) {
	pthread_mutex_lock(&_lock);
	_clients.insert(cc);
	pthread_mutex_unlock(&_lock);
}

void EventRegister::Event::deregisterClient(ClientConnection *cc) {
	pthread_mutex_lock(&_lock);
	ClientConnectionPool::iterator i = _clients.find(cc);
	if(i != _clients.end())
		_clients.erase(i);
	pthread_mutex_unlock(&_lock);
}

bool EventRegister::Event::isClientRegistered(ClientConnection *cc) {
    bool result;
    pthread_mutex_lock(&_lock);
    ClientConnectionPool::iterator i = _clients.find(cc);
    if (i != _clients.end())
	result = true;
    else
	result = false;
    pthread_mutex_unlock(&_lock);
    return result;
}
