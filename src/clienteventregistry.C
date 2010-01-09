/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "clienteventregistry.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "logger.h"

using namespace std;

ClientEventRegistry ClientEventRegistry::_instance;

ClientEventRegistry::ClientEventRegistry() {
	pthread_mutex_init(&_lock, NULL);
	_clients.clear();
}

ClientEventRegistry::~ClientEventRegistry() {
	pthread_mutex_destroy(&_lock);
	EventMap::iterator i;
	for(i = _eventmap.begin(); i != _eventmap.end(); ++i)
		delete i->second;
}

void ClientEventRegistry::registerClient(ClientConnection *cc) {
	if (cc == NULL) {
		log(LOG_ERR, "F**K!!!! A NULL ClientConnection!!!");
		return;
	}
	uint32_t user_id = cc->getProperty("user_id");
	pthread_mutex_lock(&_lock);
	_clients[user_id] = cc;
	pthread_mutex_unlock(&_lock);
};

bool ClientEventRegistry::registerClient(string eventname, ClientConnection *cc) {
	EventMap::iterator i = _eventmap.find(eventname);
	if(i == _eventmap.end()) {
		log(LOG_INFO, "Attempt to register for non-existant event '%s' from user %u.", eventname.c_str(), cc->getProperty("user_id"));
		return false;
	}
	i->second->registerClient(cc);
	return true;
}

void ClientEventRegistry::deregisterClient(ClientConnection *cc) {
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

void ClientEventRegistry::deregisterClient(string eventname, ClientConnection *cc) {
	pthread_mutex_lock(&_lock);
	Event* ev = _eventmap[eventname];
	log(LOG_DEBUG, "Deregistering from event '%s'", eventname.c_str());
	if(!ev)
		log(LOG_INFO, "Attempt by user %d to deregister from non-existant event '%s'", cc->getProperty("user_id"), eventname.c_str());
	else
		ev->deregisterClient(cc);
	pthread_mutex_unlock(&_lock);
}

bool ClientEventRegistry::isClientRegistered(string eventname, ClientConnection *cc) {
	bool ans;
	pthread_mutex_lock(&_lock);

	EventMap::iterator i = _eventmap.find(eventname);
	if(i == _eventmap.end()) {
		log(LOG_INFO, "Attempt by user %d to check registration for non-existant event '%s'", cc->getProperty("user_id"), eventname.c_str());
		ans = false;
	}
	else
		ans = i->second->isClientRegistered(cc);
	pthread_mutex_unlock(&_lock);
	return ans;
}

void ClientEventRegistry::registerEvent(string eventname) {
	pthread_mutex_lock(&_lock);
	Event* ev = _eventmap[eventname];
	if(ev)
		log(LOG_NOTICE, "Attempt to re-register event '%s'", eventname.c_str());
	else
		_eventmap[eventname] = new Event();
	pthread_mutex_unlock(&_lock);
}

void ClientEventRegistry::triggerEvent(string eventname, const MessageBlock *mb) {
	pthread_mutex_lock(&_lock);
	EventMap::iterator i = _eventmap.find(eventname);
	pthread_mutex_unlock(&_lock);
	if(i == _eventmap.end())
		log(LOG_ERR, "Attempt to trigger unknown event '%s'",
				eventname.c_str());
	else
		i->second->triggerEvent(mb);
}

void ClientEventRegistry::broadcastEvent(const MessageBlock *mb) {
	pthread_mutex_lock(&_lock);
	for(ClientMap::iterator i = _clients.begin(); i != _clients.end(); ++i) {
		log(LOG_INFO, "broadcasting to client %p (with id %d)", i->second, i->first);
		if (i->second != NULL)
			i->second->sendMessageBlock(mb);
	}
	pthread_mutex_unlock(&_lock);
}

void ClientEventRegistry::sendMessage(uint32_t user_id, const MessageBlock *mb) {
	pthread_mutex_lock(&_lock);
	ClientMap::iterator i = _clients.find(user_id);
	if (i != _clients.end()) {
		ClientConnection *cc = i->second;
		if(cc)
			cc->sendMessageBlock(mb);
	}
	pthread_mutex_unlock(&_lock);
}

ClientEventRegistry& ClientEventRegistry::getInstance() {
	return _instance;
}

ClientEventRegistry::Event::Event() {
	pthread_mutex_init(&_lock, NULL);
}

ClientEventRegistry::Event::~Event() {
	pthread_mutex_destroy(&_lock);
}

void ClientEventRegistry::Event::triggerEvent(const MessageBlock *mb) {
	pthread_mutex_lock(&_lock);
	ClientConnectionPool::iterator i;
	for(i = _clients.begin(); i != _clients.end(); ++i)
		(*i)->sendMessageBlock(mb);
	pthread_mutex_unlock(&_lock);
}

void ClientEventRegistry::Event::registerClient(ClientConnection *cc) {
	pthread_mutex_lock(&_lock);
	_clients.insert(cc);
	pthread_mutex_unlock(&_lock);
}

void ClientEventRegistry::Event::deregisterClient(ClientConnection *cc) {
	pthread_mutex_lock(&_lock);
	ClientConnectionPool::iterator i = _clients.find(cc);
	if(i != _clients.end())
		_clients.erase(i);
	pthread_mutex_unlock(&_lock);
}

bool ClientEventRegistry::Event::isClientRegistered(ClientConnection *cc) {
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
