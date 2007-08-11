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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "supportmodule.h"
#include "scoped_lock.h"

#include <pthread.h>
#include <stdint.h>
#include <map>
#include <vector>

#define USER_TYPE_NONE			0
#define USER_TYPE_ADMIN			1
#define USER_TYPE_JUDGE			2
#define USER_TYPE_CONTESTANT	3

class UserSupportModule : public SupportModule {
public:
	struct User {
		uint32_t user_id;
		std::string username;
//		std::string friendlyname; TODO
		uint32_t type;
	};

	typedef std::vector<User> UserList;
private:
/*	typedef std::map<uint32_t, struct User> UserMap;
	UserMap _users;
	pthread_mutex_t _id_lock;
	pthread_mutex_t _usermap_lock;

	UserMap::const_iterator getUser(uint32_t user_id); */
public:
	UserSupportModule();
	virtual ~UserSupportModule();

	virtual void init();

	UserList list();
	uint32_t __attribute__((pure)) user_id(const std::string& username);
	std::string __attribute__((pure)) username(uint32_t user_id);
	uint32_t __attribute__((pure)) usertype(uint32_t user_id);
	uint32_t nextId();

	std::string __attribute__((pure)) hashpw(uint32_t user_id, const std::string& pw);
	std::string __attribute__((pure)) hashpw(const std::string& uname, const std::string& pw);
};

DEFINE_SUPPORT_MODULE_GETTER(UserSupportModule);

#endif
