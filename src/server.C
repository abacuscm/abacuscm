/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "server.h"
#include "dbcon.h"
#include "acmconfig.h"
#include "logger.h"
#include "peermessenger.h"
#include "message.h"
#include "queue.h"
#include "socket.h"

#include <pthread.h>
#include <stdlib.h>
#include <sstream>

using namespace std;

#define ID_GRANULARITY		MAX_SERVERS

static Queue<uint32_t> *ack_queue;
static Queue<TimedAction*> *timed_queue;
static WaitableSet *waitable_set;

uint32_t Server::server_id(const string& name)
{
	DbCon *db = DbCon::getInstance();
	if (!db)
		return ~0U;

	ostringstream query;
	query << "SELECT server_id FROM Server WHERE server_name='" << db->escape_string(name) << "'";

	QueryResultRow r = db->singleRowQuery(query.str());
	db->release();

	return r.size() ? from_string<uint32_t>(r[0]) : 0;
}

string Server::servername(uint32_t server_id)
{
	DbCon *db = DbCon::getInstance();
	if (!db)
		return "";

	ostringstream query;
	query << "SELECT server_name FROM Server WHERE server_id=" << server_id;

	QueryResultRow r = db->singleRowQuery(query.str());
	db->release();

	return r.size() ? *r.begin() : "";
}

uint32_t Server::getId() {
	Config &config = Config::getConfig();
	static uint32_t local_id = 0;
	if(!local_id) {
		log(LOG_DEBUG, "Initialising local_id");
		DbCon *db = DbCon::getInstance();
		if(!db)
			return ~0U;
		local_id = server_id(config["initialisation"]["name"]);
		db->release();db=NULL;
		if(local_id == 0 && config["initialisation"]["type"] == "master")
			local_id = 1;
		log(LOG_DEBUG, "local_id=%d", local_id);
	}
	return local_id;
}

uint32_t Server::nextSubmissionId() {
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	static uint32_t cur_max_id = 0;

	uint32_t local_max_id;

	pthread_mutex_lock(&lock);

	if(!cur_max_id) {
		DbCon *db = DbCon::getInstance();
		if(!db)
			goto err;
		cur_max_id = db->maxSubmissionId();
		db->release();db=NULL;

		if(cur_max_id == ~0U) {
			cur_max_id = 0;
			goto err;
		}
	}

	if(!cur_max_id)
		cur_max_id = getId();
	else
		cur_max_id += ID_GRANULARITY;

	local_max_id = cur_max_id;

	pthread_mutex_unlock(&lock);

	return local_max_id;
err:
	pthread_mutex_unlock(&lock);
	return ~0U;
}

uint32_t Server::nextClarificationRequestId() {
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	static uint32_t cur_max_id = 0;

	uint32_t local_max_id;

	pthread_mutex_lock(&lock);

	if(!cur_max_id) {
		DbCon *db = DbCon::getInstance();
		if(!db)
			goto err;
		cur_max_id = db->maxClarificationReqId();
		db->release();db=NULL;

		if(cur_max_id == ~0U) {
			cur_max_id = 0;
			goto err;
		}
	}

	if(!cur_max_id)
		cur_max_id = getId();
	else
		cur_max_id += ID_GRANULARITY;

	local_max_id = cur_max_id;

	pthread_mutex_unlock(&lock);

	return local_max_id;
err:
	pthread_mutex_unlock(&lock);
	return ~0U;
}

uint32_t Server::nextClarificationId() {
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	static uint32_t cur_max_id = 0;

	uint32_t local_max_id;

	pthread_mutex_lock(&lock);

	if(!cur_max_id) {
		DbCon *db = DbCon::getInstance();
		if(!db)
			goto err;
		cur_max_id = db->maxClarificationId();
		db->release();db=NULL;

		if(cur_max_id == ~0U) {
			cur_max_id = 0;
			goto err;
		}
	}

	if(!cur_max_id)
		cur_max_id = getId();
	else
		cur_max_id += ID_GRANULARITY;

	local_max_id = cur_max_id;

	pthread_mutex_unlock(&lock);

	return local_max_id;
err:
	pthread_mutex_unlock(&lock);
	return ~0U;
}

uint32_t Server::nextServerId() {
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	static uint32_t cur_max_id = 0;

	uint32_t local_max_id;

	pthread_mutex_lock(&lock);

	if(!cur_max_id) {
		DbCon *db = DbCon::getInstance();
		if(!db)
			goto err;
		cur_max_id = db->maxServerId();
		db->release();db=NULL;

		if(cur_max_id == ~0U) {
			cur_max_id = 0;
			goto err;
		}
	}

	if(!cur_max_id) {
		log(LOG_ERR, "Right, we must _always_ have a cur_max_id in %s as we _must_ always have at least one server!", __PRETTY_FUNCTION__);
		goto err;
	} else
		cur_max_id += 1;

	local_max_id = cur_max_id;

	pthread_mutex_unlock(&lock);

	return local_max_id;
err:
	pthread_mutex_unlock(&lock);
	return ~0U;
}

uint32_t Server::nextProblemId() {
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	static uint32_t cur_max_id = 0;

	uint32_t local_max_id;

	pthread_mutex_lock(&lock);

	if(!cur_max_id) {
		DbCon *db = DbCon::getInstance();
		if(!db)
			goto err;
		cur_max_id = db->maxProblemId();
		db->release();db=NULL;

		if(cur_max_id == ~0U) {
			cur_max_id = 0;
			goto err;
		}
	}

	if(!cur_max_id)
		cur_max_id = getId();
	else
		cur_max_id += ID_GRANULARITY;

	local_max_id = cur_max_id;

	pthread_mutex_unlock(&lock);

	return local_max_id;
err:
	pthread_mutex_unlock(&lock);
	return ~0U;
}

bool Server::hasMessage(uint32_t server_id, uint32_t message_id) {
	DbCon *db = DbCon::getInstance();
	if(!db) {
		log(LOG_ERR, "Unable to check for existence of message (%u,%u)",
				server_id, message_id);
		return false;
	}
	bool res = db->hasMessage(server_id, message_id);
	db->release();db=NULL;
	return res;
}

void Server::putAck(uint32_t server_id, uint32_t message_id, uint32_t ack_id) {
	if(!ack_id) {
		log(LOG_ERR, "ack_id == 0 cannot possibly be correct.  This could potentially happen if/when a server did not initialise properly upon first creation (the first PeerMessage a server receives must be its own initialisation message.  Please see the administrator's manual for more information.");
		return;
	}

	if(ack_queue)
		ack_queue->enqueue(ack_id);

	if(server_id && message_id) {
		DbCon *db = DbCon::getInstance();
		if(!db) {
			log(LOG_WARNING, "Not committing ACK for (%u,%u) from %u to DB",
					server_id, message_id, ack_id);
			return;
		}
		db->ackMessage(server_id, message_id, ack_id);
		db->release();db=NULL;
	}
}

void Server::setAckQueue(Queue<uint32_t>* _ack_queue) {
	ack_queue = _ack_queue;
}

void Server::setTimedQueue(Queue<TimedAction*>* _timed_queue) {
	timed_queue = _timed_queue;
}

void Server::putTimedAction(TimedAction* ta) {
	if(!timed_queue)
		log(LOG_ERR, "timed_queue is not set!");
	else
		timed_queue->enqueue(ta);
}

void Server::putWaitable(Waitable* s, bool immediate)
{
	if (immediate) {
		log(LOG_ERR, "TODO: putWaitable(immediate=true) not implemented!");
	} else {
		if (!waitable_set)
			log(LOG_ERR, "waitable_set is not set!");
		else
			waitable_set->add(s);
	}
}

void Server::setWaitableSet(WaitableSet *set)
{
	waitable_set = set;
}
