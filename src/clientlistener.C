#include "clientlistener.h"
#include "logger.h"

ClientListener::ClientListener() {
	_pool = NULL;
}

ClientListener::~ClientListener() {
}

bool ClientListener::init(SocketPool *pool) {
	_pool = pool;
	NOT_IMPLEMENTED();
	return true;
}

bool ClientListener::process() {
	NOT_IMPLEMENTED();
	return false;
}
