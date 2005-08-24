#include "serverconnection.h"
#include "logger.h"

using namespace std;

ServerConnection::ServerConnection() {
}

ServerConnection::~ServerConnection() {
}

bool ServerConnection::connect(string, int) {
	NOT_IMPLEMENTED();
	return false;
}

bool ServerConnection::disconnect() {
	NOT_IMPLEMENTED();
	return false;
}
	
bool ServerConnection::registerEventCallback(string event, EventCallback func, void *custom) {
	NOT_IMPLEMENTED();
	return false;
}

bool ServerConnection::deregisterEventCallback(string event, EventCallback func) {
	NOT_IMPLEMENTED();
	return false;
}
