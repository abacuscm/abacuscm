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
		log(LOG_ERR, "A NULL ClientConnection!!!");
		return;
	}
	uint32_t user_id = cc->getUserId();
	pthread_mutex_lock(&_lock);
	_clients.insert(make_pair(user_id, cc));
	pthread_mutex_unlock(&_lock);
};

bool ClientEventRegistry::registerClient(const string &eventname, ClientConnection *cc) {
	bool success = false;
	pthread_mutex_lock(&_lock);
	EventMap::iterator i = _eventmap.find(eventname);
	if(i == _eventmap.end())
		log(LOG_INFO, "Attempt to register for non-existent event '%s' from user %u.", eventname.c_str(), cc->getUserId());
	else if (!i->second->registerClient(cc))
		log(LOG_INFO, "Attempt to register for non-permitted event '%s' from user %u.", eventname.c_str(), cc->getUserId());
	else
		success = true;
	pthread_mutex_unlock(&_lock);
	return success;
}

void ClientEventRegistry::deregisterClient(ClientConnection *cc) {
	uint32_t user_id = cc->getUserId();
	pthread_mutex_lock(&_lock);

	EventMap::iterator e;
	for(e = _eventmap.begin(); e != _eventmap.end(); ++e)
		e->second->deregisterClient(cc);

	pair<ClientMap::iterator, ClientMap::iterator> range = _clients.equal_range(user_id);
	ClientMap::iterator c = range.first;
	while (c != range.second) {
		ClientMap::iterator next = c;
		++next;
		if (c->second == cc)
			_clients.erase(c);
		c = next;
	}
	pthread_mutex_unlock(&_lock);
}

bool ClientEventRegistry::deregisterClient(const string &eventname, ClientConnection *cc) {
	bool success = false;
	pthread_mutex_lock(&_lock);
	log(LOG_DEBUG, "Deregistering from event '%s'", eventname.c_str());
	EventMap::iterator i = _eventmap.find(eventname);
	if(i == _eventmap.end())
		log(LOG_INFO, "Attempt by user %u to deregister from non-existent event '%s'", cc->getUserId(), eventname.c_str());
	else if (!i->second->deregisterClient(cc))
		log(LOG_INFO, "Attempt by user %u to deregister from non-existent event '%s'", cc->getUserId(), eventname.c_str());
	else
		success = true;
	pthread_mutex_unlock(&_lock);
	return success;
}

bool ClientEventRegistry::isClientRegistered(const string &eventname, ClientConnection *cc) {
	bool ans;
	pthread_mutex_lock(&_lock);

	EventMap::iterator i = _eventmap.find(eventname);
	if(i == _eventmap.end()) {
		log(LOG_INFO, "Attempt by user %d to check registration for non-existant event '%s'", cc->getUserId(), eventname.c_str());
		ans = false;
	}
	else
		ans = i->second->isClientRegistered(cc);
	pthread_mutex_unlock(&_lock);
	return ans;
}

void ClientEventRegistry::registerEvent(const string &eventname, const PermissionTest &pt) {
	pthread_mutex_lock(&_lock);
	Event* ev = _eventmap[eventname];
	if(ev)
		log(LOG_NOTICE, "Attempt to re-register event '%s'", eventname.c_str());
	else
		_eventmap[eventname] = new Event(pt);
	pthread_mutex_unlock(&_lock);
}

void ClientEventRegistry::triggerEvent(const string &eventname, const MessageBlock *mb) {
	pthread_mutex_lock(&_lock);
	EventMap::iterator i = _eventmap.find(eventname);
	pthread_mutex_unlock(&_lock);
	if(i == _eventmap.end())
		log(LOG_ERR, "Attempt to trigger unknown event '%s'",
				eventname.c_str());
	else
		i->second->triggerEvent(mb);
}

void ClientEventRegistry::broadcastEvent(uint32_t user_id, const PermissionTest &pt, const MessageBlock *mb) {
	pthread_mutex_lock(&_lock);
	for(ClientMap::iterator i = _clients.begin(); i != _clients.end(); ++i) {
		if (i->first == user_id || pt.matches(i->second->permissions())) {
			log(LOG_INFO, "broadcasting to client %p (with id %d)", i->second, i->first);
			i->second->sendMessageBlock(mb);
		}
	}
	pthread_mutex_unlock(&_lock);
}

void ClientEventRegistry::sendMessage(uint32_t user_id, const MessageBlock *mb) {
	pthread_mutex_lock(&_lock);
	pair<ClientMap::const_iterator, ClientMap::const_iterator> range = _clients.equal_range(user_id);
	for (ClientMap::const_iterator i = range.first; i != range.second; ++i) {
		ClientConnection *cc = i->second;
		if(cc)
			cc->sendMessageBlock(mb);
	}
	pthread_mutex_unlock(&_lock);
}

ClientEventRegistry& ClientEventRegistry::getInstance() {
	return _instance;
}

ClientEventRegistry::Event::Event(const PermissionTest &pt) : _pt(pt) {
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

bool ClientEventRegistry::Event::registerClient(ClientConnection *cc) {
	if (!_pt.matches(cc->permissions())) {
		return false;
	}
	pthread_mutex_lock(&_lock);
	_clients.insert(cc);
	pthread_mutex_unlock(&_lock);
	return true;
}

bool ClientEventRegistry::Event::deregisterClient(ClientConnection *cc) {
	if (!_pt.matches(cc->permissions())) {
		return false;
	}
	pthread_mutex_lock(&_lock);
	ClientConnectionPool::iterator i = _clients.find(cc);
	if(i != _clients.end())
		_clients.erase(i);
	pthread_mutex_unlock(&_lock);
	return true;
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
