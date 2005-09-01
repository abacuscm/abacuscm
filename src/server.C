#include "server.h"
#include "dbcon.h"
#include "config.h"
#include "logger.h"
#include "peermessenger.h"
#include "message.h"

#include <pthread.h>

#define ID_GRANULARITY		MAX_SERVERS

uint32_t Server::getId() {
	Config &config = Config::getConfig();
	static uint32_t local_id = 0;
	if(!local_id) {
		log(LOG_DEBUG, "Initialising local_id");
		DbCon *db = DbCon::getInstance();
		if(!db)
			return ~0U;
		local_id = db->name2server_id(config["initialisation"]["name"]);
		db->release();
		if(local_id == 0 && config["initialisation"]["type"] == "master")
			local_id = 1;
		log(LOG_DEBUG, "local_id=%d", local_id);
	}
	return local_id;
}

uint32_t Server::nextUserId() {
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	static uint32_t cur_max_id = 0;

	uint32_t local_max_id;
	
	pthread_mutex_lock(&lock);

	if(!cur_max_id) {
		DbCon *db = DbCon::getInstance();
		if(!db)
			goto err;
		cur_max_id = db->maxUserId();
		db->release();

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
		db->release();

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

bool Server::hasMessage(uint32_t, uint32_t) {
	NOT_IMPLEMENTED();
	return false;
}

void Server::flushMessages(uint32_t server_id) {
	DbCon *db = DbCon::getInstance();
	if(!db) {
		log(LOG_WARNING, "Failed to flush queued messages for %u", server_id);
		return;
	}
	
	MessageList msglist = db->getUnacked(server_id);
	db->release();

	PeerMessenger *messenger = PeerMessenger::getMessenger();
	MessageList::iterator i;
	for(i = msglist.begin(); i != msglist.end(); ++i) {
		messenger->sendMessage(server_id, *i);
		delete *i;
	}
}
