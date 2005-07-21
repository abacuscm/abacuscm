#include "dbcon.h"
#include "logger.h"

#include <queue>
#include <pthread.h>

using namespace std;

static queue<DbCon*> _con_stack;
static DbCon * (*_functor)() = 0;
static pthread_mutex_t _lock = PTHREAD_MUTEX_INITIALIZER;

DbCon::DbCon() {
}

DbCon::~DbCon() {
}

DbCon* DbCon::getInstance() {
	DbCon *tmp = 0;
	pthread_mutex_lock(&_lock);
	if(_con_stack.empty()) {
		if(_functor)
			tmp = _functor();
		else
			log(LOG_ERR, "No functor specified to create DB Connections!@");
	} else {
		tmp = _con_stack.front();
		_con_stack.pop();
	}
	pthread_mutex_unlock(&_lock);
	if(tmp && !tmp->ok()) {
		delete tmp;
		return getInstance();
	}
	if(!tmp)
		log(LOG_ERR, "Error creating DB Connection!");
	return tmp;
}

void DbCon::release() {
	pthread_mutex_lock(&_lock);
	_con_stack.push(this);
	pthread_mutex_unlock(&_lock);
}

bool DbCon::registerFunctor(DbCon* (*functor)()) {
	_functor = functor;
	return true;
}
