/**
 * Copyright (c) 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */

#include "acmconfig.h"
#include "permissionmap.h"

using namespace std;

const PermissionMap PermissionMap::_instance;

void PermissionMap::allow(Permission perm, uint32_t mask) {
	uint32_t type = 0;
	while (mask > 0)
	{
		if (mask & 1)
		{
			UserType user_type = static_cast<UserType>(type);
			_map[user_type][static_cast<size_t>(perm)] = true;
		}
		mask >>= 1;
		type++;
	}
}

PermissionMap::PermissionMap() {
	// TODO: eventually allow it to be configured from the config file

	allow(PERMISSION_AUTH,                           USER_MASK_ALL);
	allow(PERMISSION_SUBMIT,                         USER_MASK_CONTESTANT | USER_MASK_OBSERVER);
	allow(PERMISSION_CLARIFICATION_REQUEST,          USER_MASK_NOMARKER);
	allow(PERMISSION_CLARIFICATION_REPLY,            USER_MASK_MANAGER);
	allow(PERMISSION_CHANGE_PASSWORD,                USER_MASK_NOMARKER);
	allow(PERMISSION_IN_STANDINGS,                   USER_MASK_CONTESTANT);
	allow(PERMISSION_SEE_STANDINGS,                  USER_MASK_NOMARKER | USER_MASK_NONE);
	allow(PERMISSION_SEE_ALL_STANDINGS,              USER_MASK_OBSERVER | USER_MASK_PROCTOR | USER_MASK_MANAGER);
	allow(PERMISSION_SEE_FINAL_STANDINGS,            USER_MASK_MANAGER | USER_MASK_PROCTOR);
	allow(PERMISSION_SEE_ALL_CLARIFICATION_REQUESTS, USER_MASK_MANAGER);
	allow(PERMISSION_SEE_ALL_CLARIFICATIONS,         USER_MASK_MANAGER);
	allow(PERMISSION_SEE_ALL_SUBMISSIONS,            USER_MASK_MANAGER);
	allow(PERMISSION_SEE_ALL_PROBLEMS,               USER_MASK_MANAGER);
	allow(PERMISSION_SEE_PROBLEM_DETAILS,            USER_MASK_MANAGER | USER_MASK_MARKER);
	allow(PERMISSION_SEE_SUBMISSION_DETAILS,         USER_MASK_MANAGER);
	allow(PERMISSION_SEE_USER_ID,                    USER_MASK_ADMIN);
	allow(PERMISSION_MARK,                           USER_MASK_MARKER | USER_MASK_ADMIN);
	allow(PERMISSION_JUDGE,                          USER_MASK_MANAGER);
	allow(PERMISSION_JUDGE_OVERRIDE,                 USER_MASK_ADMIN);
	allow(PERMISSION_SET_BONUS,                      USER_MASK_MANAGER);
	allow(PERMISSION_USER_ADMIN,                     USER_MASK_ADMIN);
	allow(PERMISSION_SERVER_ADMIN,                   USER_MASK_ADMIN);
	allow(PERMISSION_PROBLEM_ADMIN,                  USER_MASK_ADMIN);
	allow(PERMISSION_START_STOP,                     USER_MASK_ADMIN);
}

PermissionMap::~PermissionMap() {
}

const PermissionMap *PermissionMap::getInstance() {
	return &_instance;
}

const PermissionSet &PermissionMap::getPermissions(UserType user_type) const {
	static const PermissionSet empty_set;
	map<UserType, PermissionSet>::const_iterator pos;
	pos = _map.find(user_type);
	if (pos != _map.end())
		return pos->second;
	else
		return empty_set;
}
