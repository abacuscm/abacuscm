#include "server.h"
#include "dbcon.h"
#include "config.h"
#include "logger.h"

uint32_t Server::getId() {
	static uint32_t local_id = 0;
	if(!local_id) {
		log(LOG_DEBUG, "Initialising local_id");
		DbCon *db = DbCon::getInstance();
		if(!db)
			return ~0U;
		local_id = db->name2server_id(Config::getConfig()["initialisation"]["name"]);
		db->release();
		log(LOG_DEBUG, "local_id=%d", local_id);
	}
	return local_id;
}		
