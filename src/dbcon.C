/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "dbcon.h"
#include "logger.h"

#include <queue>
#include <map>
#include <execinfo.h>
#include <pthread.h>

using namespace std;

struct dbcon_bt_info {
	int bt_size;
	void* bt[5];
};

typedef map<DbCon*, struct dbcon_bt_info> dbcon_bt_map;

static queue<DbCon*> _con_queue;
static DbCon * (*_functor)() = 0;
static pthread_mutex_t _lock = PTHREAD_MUTEX_INITIALIZER;
static dbcon_bt_map _dbcon_bt;

DbCon::DbCon() {
}

DbCon::~DbCon() {
}

DbCon* DbCon::getInstance() {
	DbCon *tmp = 0;
	pthread_mutex_lock(&_lock);
	if(_con_queue.empty()) {
		if(_functor)
			tmp = _functor();
		else
			log(LOG_ERR, "No functor specified to create DB Connections!");
	} else {
		tmp = _con_queue.front();
		_con_queue.pop();
	}
	pthread_mutex_unlock(&_lock);
	if(tmp && !tmp->ok()) {
		delete tmp;
		return getInstance();
	}
	if(!tmp)
		log(LOG_ERR, "Error creating DB Connection!");
	else {
		dbcon_bt_info bt;
		bt.bt_size = backtrace(bt.bt, sizeof(bt.bt) / sizeof(*bt.bt));
    	pthread_mutex_lock(&_lock);
		_dbcon_bt[tmp] = bt;
		pthread_mutex_unlock(&_lock);
	}
	return tmp;
}

void DbCon::release() {
    pthread_mutex_lock(&_lock);
	bool ok = true;
	dbcon_bt_map::iterator m = _dbcon_bt.find(this);
	if(m != _dbcon_bt.end()) {
		_dbcon_bt.erase(m);
	} else {
		log(LOG_ERR, "Trying to release a DB connection that isn't currently 'out there'.");
		ok = false;
	}
    if (ok)
        _con_queue.push(this);
	pthread_mutex_unlock(&_lock);
}

bool DbCon::registerFunctor(DbCon* (*functor)()) {
	_functor = functor;
	return true;
}

void DbCon::cleanup() {
	pthread_mutex_lock(&_lock);
	while(!_con_queue.empty()) {
		delete _con_queue.front();
		_con_queue.pop();
	}
	if (!_dbcon_bt.empty()) {
		log(LOG_ERR, "Leaked %lu database connections, showing stack traces:", (unsigned long) _dbcon_bt.size());
		dbcon_bt_map::iterator i;
		int tn = 0;
		for(i = _dbcon_bt.begin(); i != _dbcon_bt.end(); ++i) {
			log(LOG_ERR, "Trace number %d:", ++tn);
			char** symbols = backtrace_symbols(i->second.bt, i->second.bt_size);
			for(int s = 0; s < i->second.bt_size; ++s)
				log(LOG_ERR, "%d: %s", s, symbols[s]);
			free(symbols);
			delete i->first;
		}
	}
	pthread_mutex_unlock(&_lock);
}
