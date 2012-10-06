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

class UserSupportModule : public SupportModule {
	DECLARE_SUPPORT_MODULE(UserSupportModule);
public:
	struct User {
		uint32_t user_id;
		std::string username;
//		std::string friendlyname; TODO
		uint32_t type;
	};

	typedef std::vector<User> UserList;
private:
	UserSupportModule();
	virtual ~UserSupportModule();

/*	typedef std::map<uint32_t, struct User> UserMap;
	UserMap _users;
	pthread_mutex_t _id_lock;
	pthread_mutex_t _usermap_lock;

	UserMap::const_iterator getUser(uint32_t user_id); */
public:
	virtual void init();

	virtual bool addUser(uint32_t user_id, const std::string& username, const std::string& friendlyname, const std::string& password, uint32_t type);
	virtual bool addGroup(uint32_t group_id, const std::string& groupname);
	UserList list();
	uint32_t user_id(const std::string& username);
	uint32_t group_id(const std::string& groupname);
	std::string username(uint32_t user_id);
	std::string friendlyname(uint32_t user_id);
	uint32_t usertype(uint32_t user_id);
	uint32_t nextId(); // next free user ID
	uint32_t nextGroupId(); // next free group ID

	std::string __attribute__((pure)) hashpw(uint32_t user_id, const std::string& pw);
};

DEFINE_SUPPORT_MODULE_GETTER(UserSupportModule);

#endif
