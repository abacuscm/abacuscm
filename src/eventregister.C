#include "eventregister.h"
#include "clientconnection.h"
#include "messageblock.h"

using namespace std;

EventRegister EventRegister::_instance;

EventRegister::EventRegister() {
	pthread_mutex_init(&_lock, NULL);
}

EventRegister::~EventRegister() {
	pthread_mutex_destroy(&_lock);
}

void EventRegister::registerClient(ClientConnection *cc) {
	uint32_t user_id = cc->getProperty("user_id");
	pthread_mutex_lock(&_lock);
	_clients[user_id] = cc;
	pthread_mutex_unlock(&_lock);
};

void EventRegister::deregisterClient(ClientConnection *cc) {
	uint32_t user_id = cc->getProperty("user_id");
	pthread_mutex_lock(&_lock);
	ClientMap::iterator i = _clients.find(user_id);
	if(i != _clients.end())
		_clients.erase(i);
	pthread_mutex_unlock(&_lock);
}
