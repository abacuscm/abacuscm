#include "usersupportmodule.h"
#include "server.h"

uint32_t UserSupportModule::nextId() {
	ScopedLock _l(_id_lock);

	if(!_cur_max_id) {
		DbCon *db = DbCon::getInstance();
		if(!db)
			return ~0U;
		_cur_max_id = db->maxUserId(); // TODO
		db->release();db=NULL;

		if(_cur_max_id == ~0U) {
			_cur_max_id = 0;
			return ~0U;
		}
	}

	if(!cur_max_id)
		cur_max_id = Server::getId();
	else
		cur_max_id += ID_GRANULARITY;

	return cur_max_id;
}
