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
#include <pthread.h>

using namespace std;

static vector<DbCon*> _con_stack;
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
			log(LOG_ERR, "No functor specified to create DB Connections!");
	} else {
		tmp = _con_stack.back();
		_con_stack.pop_back();
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
    bool ok = true;
    for (vector<DbCon *>::iterator i = _con_stack.begin(); i != _con_stack.end(); i++)
        if (*i == this) {
            log(LOG_ERR, "F**k, trying to release an already released DB connection!!!");
            ok = false;
            break;
        }
    if (ok)
        _con_stack.push_back(this);
	pthread_mutex_unlock(&_lock);
}

bool DbCon::registerFunctor(DbCon* (*functor)()) {
	_functor = functor;
	return true;
}

void DbCon::cleanup() {
	pthread_mutex_lock(&_lock);
	while(!_con_stack.empty()) {
		delete _con_stack.back();
		_con_stack.pop_back();
	}
	pthread_mutex_unlock(&_lock);
}
