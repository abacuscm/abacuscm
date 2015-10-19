/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "usersupportmodule.h"
#include "dbcon.h"
#include "server.h"
#include "hashpw.h"

#include <stdlib.h>
#include <sstream>

using namespace std;

DEFINE_SUPPORT_MODULE(UserSupportModule);

UserSupportModule::UserSupportModule()
{
}

UserSupportModule::~UserSupportModule()
{
}

void UserSupportModule::init()
{
}

string UserSupportModule::username(uint32_t user_id)
{
	DbCon *db = DbCon::getInstance();
	if (!db)
		return "";

	ostringstream query;
	query << "SELECT username FROM User WHERE user_id=" << user_id;

	QueryResultRow res = db->singleRowQuery(query.str());
	db->release();

	if (res.size())
		return *res.begin();
	return "";
}

string UserSupportModule::groupname(uint32_t group_id)
{
	DbCon *db = DbCon::getInstance();
	if (!db)
		return "";

	ostringstream query;
	query << "SELECT groupname FROM `Group` WHERE group_id=" << group_id;

	QueryResultRow res = db->singleRowQuery(query.str());
	db->release();

	if (res.size())
		return *res.begin();
	return "";
}

uint32_t UserSupportModule::user_id(const string& username)
{
	DbCon *db = DbCon::getInstance();
	if (!db)
		return ~0U;

	ostringstream query;
	query << "SELECT user_id FROM User WHERE username='" << db->escape_string(username) << "'";

	QueryResultRow res = db->singleRowQuery(query.str());
	db->release();

	if (res.size())
		return strtoul(res.begin()->c_str(), NULL, 10);
	return 0;
}

uint32_t UserSupportModule::group_id(const string& groupname)
{
	DbCon *db = DbCon::getInstance();
	if (!db)
		return ~0U;

	ostringstream query;
	query << "SELECT group_id FROM `Group` WHERE groupname='" << db->escape_string(groupname) << "'";

	QueryResultRow res = db->singleRowQuery(query.str());
	db->release();

	if (res.size())
		return strtoul(res.begin()->c_str(), NULL, 10);
	return 0;
}

string UserSupportModule::friendlyname(uint32_t user_id)
{
	DbCon *db = DbCon::getInstance();
	if (!db)
		return "";

	ostringstream query;
	query << "SELECT friendlyname FROM User WHERE user_id=" << user_id;

	QueryResultRow res = db->singleRowQuery(query.str());
	db->release();

	if (res.size())
		return *res.begin();
	return "";
}

uint32_t UserSupportModule::usertype(uint32_t user_id)
{
	DbCon *db = DbCon::getInstance();
	if (!db)
		return ~0U;

	ostringstream query;
	query << "SELECT type FROM User WHERE user_id=" << user_id;

	QueryResultRow res = db->singleRowQuery(query.str());
	db->release();

	if (res.size())
		return strtoul(res.begin()->c_str(), NULL, 10);
	return ~0U;
}

uint32_t UserSupportModule::user_group(uint32_t user_id)
{
	DbCon *db = DbCon::getInstance();
	if (!db)
		return ~0U;

	ostringstream query;
	query << "SELECT `group` FROM User WHERE user_id=" << user_id;

	QueryResultRow res = db->singleRowQuery(query.str());
	db->release();

	if (res.size())
		return strtoul(res.begin()->c_str(), NULL, 10);
	return ~0U;
}

bool UserSupportModule::user_bonus(uint32_t user_id, int32_t &points, int32_t &seconds) {
	DbCon *db = DbCon::getInstance();
	if (!db)
		return false;

	ostringstream query;
	query << "SELECT bonus_points, bonus_seconds from User WHERE user_id=" << user_id;

	QueryResultRow res = db->singleRowQuery(query.str());
	db->release();

	if (res.size()) {
		points = strtol(res[0].c_str(), NULL, 10);
		seconds = strtol(res[1].c_str(), NULL, 10);
		return true;
	}
	return false;
}

uint32_t UserSupportModule::nextId()
{
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	static uint32_t cur_max_id = 0;

	uint32_t local_max_id;

	pthread_mutex_lock(&lock);

	if(!cur_max_id) {
		DbCon *db = DbCon::getInstance();
		if(!db)
			goto err;
		cur_max_id = db->maxUserId();
		db->release();db=NULL;

		if(cur_max_id == ~0U) {
			cur_max_id = 0;
			goto err;
		}
	}

	if(!cur_max_id)
		cur_max_id = Server::getId();
	else
		cur_max_id += ID_GRANULARITY;

	local_max_id = cur_max_id;

	pthread_mutex_unlock(&lock);

	return local_max_id;
err:
	pthread_mutex_unlock(&lock);
	return ~0U;
}

uint32_t UserSupportModule::nextGroupId()
{
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	static uint32_t cur_max_id = 0;

	uint32_t local_max_id;

	pthread_mutex_lock(&lock);

	if (!cur_max_id) {
		DbCon *db = DbCon::getInstance();
		if (!db)
			goto err;
		cur_max_id = db->maxGroupId();
		db->release(); db = NULL;

		if (cur_max_id == ~0U) {
			cur_max_id = 0;
			goto err;
		}
	}

	if (!cur_max_id)
		cur_max_id = Server::getId();
	else
		cur_max_id += ID_GRANULARITY;

	local_max_id = cur_max_id;

	pthread_mutex_unlock(&lock);

	return local_max_id;
err:
	pthread_mutex_unlock(&lock);
	return ~0U;
}

string UserSupportModule::hashpw(uint32_t user_id, const string& pw)
{
	string username = this->username(user_id);
	if (username == "") return "";

	return ::hashpw(username, pw);
}

bool UserSupportModule::addUser(uint32_t user_id, const std::string& username, const std::string& friendlyname, const std::string& password, uint32_t type, uint32_t group)
{
	DbCon *db = DbCon::getInstance();
	if (!db)
		return false;

	ostringstream query;
	query << "INSERT INTO User (user_id, username, friendlyname, password, type, `group`) VALUES (" << user_id << ", '" << db->escape_string(username) << "', '" << db->escape_string(friendlyname) << "', '" << db->escape_string(password) << "', " << type << ", " << group << ")";

	bool r = db->executeQuery(query.str());
	db->release();

	return r;
}

bool UserSupportModule::addGroup(uint32_t group_id, const std::string& groupname) {
	DbCon *db = DbCon::getInstance();
	if (!db)
		return false;

	ostringstream query;
	query << "INSERT INTO `Group` (group_id, groupname) VALUES (" << group_id << ", '" << db->escape_string(groupname) << "')";

	bool r = db->executeQuery(query.str());
	db->release();

	return r;
}

UserSupportModule::UserList UserSupportModule::userList() {
	UserList res;
	DbCon *db = DbCon::getInstance();
	if (!db)
		return res;

	QueryResult qr = db->multiRowQuery("SELECT user_id, username FROM User");
	db->release();

	res.reserve(qr.size());
	for (QueryResult::iterator i = qr.begin(); i != qr.end(); ++i)
	{
		User u;

		u.user_id = strtoul((*i)[0].c_str(), NULL, 0);
		u.username = (*i)[1];

		res.push_back(u);
	}

	return res;
}

UserSupportModule::GroupList UserSupportModule::groupList() {
	GroupList res;
	DbCon *db = DbCon::getInstance();
	if (!db)
		return res;

	QueryResult qr = db->multiRowQuery("SELECT group_id, groupname FROM `Group`");
	db->release();

	res.reserve(qr.size());
	for (QueryResult::iterator i = qr.begin(); i != qr.end(); ++i)
	{
		Group g;
		g.group_id = strtoul((*i)[0].c_str(), NULL, 0);
		g.groupname = (*i)[1];
		res.push_back(g);
	}
	return res;
}
