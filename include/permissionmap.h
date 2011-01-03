/**
 * Copyright (c) 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */

#ifndef __PERMISSIONMAP_H
#define __PERMISSIONMAP_H

#include "misc.h"
#include "permissions.h"
#include <map>
#include <stdint.h>

/* Maps user types to permission sets */
class PermissionMap {
private:
	static const PermissionMap _instance;

	PermissionMap();
	~PermissionMap();

	std::map<UserType, PermissionSet> _map;

	// Adds permission to the set. The mask is an OR of
	// USER_MASK_* constants.
	void allow(Permission perm, uint32_t mask);
public:
	static const PermissionMap *getInstance();

	const PermissionSet &getPermissions(UserType user_type) const;
};

#endif
