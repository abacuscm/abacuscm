#include "eventregister.h"
#include "clientconnection.h"
#include "messageblock.h"
#include "logger.h"

using namespace std;

EventRegister EventRegister::_instance;

EventRegister::EventRegister() {
	pthread_mutex_init(&_lock, NULL);
}

EventRegister::~EventRegister() {
	pthread_mutex_destroy(&_lock);
	EventMap::iterator i;
	for(i = _eventmap.begin(); i != _eventmap.end(); ++i)
		delete i->second;
}

void EventRegister::registerClient(ClientConnection *cc) {
	uint32_t user_id = cc->getProperty("user_id");
	pthread_mutex_lock(&_lock);
	_clients[user_id] = cc;
	pthread_mutex_unlock(&_lock);
};

bool EventRegister::registerClient(string eventname, ClientConnection *cc) {
	Event* ev = _eventmap[eventname];
	if(!ev) {
		log(LOG_INFO, "Attempt to register for non-existant event '%s' from user %u.", eventname.c_str(), cc->getProperty("user_id"));
		return false;
	}
	ev->registerClient(cc);
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
	if(!ev)
		log(LOG_INFO, "Attempt by user %d to deregister from non-existant event '%s'", cc->getProperty("user_id"), eventname.c_str());
	else
		ev->deregisterClient(cc);
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
	Event* ev = _eventmap[eventname];
	if(!ev)
		log(LOG_ERR, "Attempt to trigger unknown event '%s'",
				eventname.c_str());
	else
		ev->triggerEvent(mb);
}

void EventRegister::sendMessage(uint32_t user_id, const MessageBlock *mb) {
	pthread_mutex_lock(&_lock);
	ClientConnection *cc = _clients[user_id];
	if(cc)
		cc->sendMessageBlock(mb);
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
