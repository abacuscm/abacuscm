/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __USERSUPPORTMODULE_H__
#define __USERSUPPORTMODULE_H__

#include "supportmodule.h"
#include "scoped_lock.h"

#include <pthread.h>
#include <stdint.h>
#include <map>

#define USER_TYPE_NONE			0
#define USER_TYPE_ADMIN			1
#define USER_TYPE_JUDGE			2
#define USER_TYPE_CONTESTANT	3

class UserSupportModule : public SupportModule {
public:
	struct User {
		std::string username;
		uint32_t type;
	};
private:
	typedef std::map<uint32_t, struct User> UserMap;

	UserMap _users;
	pthread_mutex_t _id_lock;
	pthread_mutex_t _usermap_lock;

	UserMap::const_iterator getUser(uint32_t user_id);
public:
	UserSupportModule();
	virtual ~UserSupportModule();

	virtual void init();

	std::string __attribute__((pure)) user2name(uint32_t user_id) {
		ScopedLock __l(&_usermap_lock);
		UserMap::const_iterator i = getUser(user_id);
		if(i != _users.end())
			return i->second.username;
		return "";
	}

	uint32_t __attribute__((pure)) user2type(uint32_t user_id) {
		ScopedLock __l(&_usermap_lock);
		UserMap::const_iterator i = getUser(user_id);
		if(i != _users.end())
			return i->second.type;
		return USER_TYPE_NONE;
	}

	uint32_t nextId();
};

DEFINE_SUPPORT_MODULE_GETTER(UserSupportModule);

#endif
