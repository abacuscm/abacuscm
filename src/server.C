#include "server.h"
#include "dbcon.h"
#include "config.h"
#include "logger.h"

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
		cur_max_id = getId() - ID_GRANULARITY;
	
	pthread_mutex_unlock(&lock);

	return cur_max_id += ID_GRANULARITY;
err:
	pthread_mutex_unlock(&lock);
	return ~0U;
}
