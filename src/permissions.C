/**
 * Copyright (c) 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */

#include "clientconnection.h"
#include "permissions.h"
#include "misc.h"
#include "acmconfig.h"
#include <utility>

using namespace std;

PermissionSet::PermissionSet()
: _type(EXPRESSION_TYPE_BASE), _base(PERMISSION_NONE), _left(NULL), _right(NULL) {
}

PermissionSet::PermissionSet(Permission perm)
: _type(EXPRESSION_TYPE_BASE), _base(perm), _left(NULL), _right(NULL) {
}

PermissionSet::PermissionSet(const PermissionSet &ps)
: _type(ps._type), _base(ps._base), _left(NULL), _right(NULL) {
	if (ps._left != NULL)
		_left = new PermissionSet(*ps._left);
	if (ps._right != NULL)
		_right = new PermissionSet(*ps._right);
}

PermissionSet::~PermissionSet() {
	delete _left;
	delete _right;
}

PermissionSet &PermissionSet::operator=(const PermissionSet &ps) {
	delete _left; _left = NULL;
	delete _right; _right = NULL;

	_type = ps._type;
	_base = ps._base;
	if (ps._left != NULL)
		_left = new PermissionSet(*ps._left);
	if (ps._right != NULL)
		_right = new PermissionSet(*ps._right);
	return *this;
}

PermissionSet PermissionSet::operator ||(const PermissionSet &ps) const {
	PermissionSet out;
	out._type = EXPRESSION_TYPE_OR;
	out._left = new PermissionSet(*this);
	out._right = new PermissionSet(ps);
	return out;
}

PermissionSet PermissionSet::operator &&(const PermissionSet &ps) const {
	PermissionSet out;
	out._type = EXPRESSION_TYPE_AND;
	out._left = new PermissionSet(*this);
	out._right = new PermissionSet(ps);
	return out;
}

PermissionSet PermissionSet::operator !() const {
	PermissionSet out;
	out._type = EXPRESSION_TYPE_NOT;
	out._left = new PermissionSet(*this);
	return out;
}

void Permissions::allow(Permission perm, uint32_t mask)
{
	uint32_t type = 0;
	while (mask > 0)
	{
		if (mask & 1)
		{
			_perms.insert(make_pair(static_cast<UserType>(type), perm));
		}
		mask >>= 1;
		type++;
	}
}

Permissions::Permissions() {
	// TODO: eventually allow it to be configured from the config file

	allow(PERMISSION_ANY,                            USER_MASK_ALL | USER_MASK_NONE);
	allow(PERMISSION_AUTH,                           USER_MASK_ALL);
	allow(PERMISSION_SUBMIT,                         USER_MASK_CONTESTANT | USER_MASK_OBSERVER);
	allow(PERMISSION_CLARIFICATION_REQUEST,          USER_MASK_NOMARKER);
	allow(PERMISSION_CLARIFICATION_REPLY,            USER_MASK_JUDGE | USER_MASK_ADMIN);
	allow(PERMISSION_CHANGE_PASSWORD,                USER_MASK_NOMARKER);
	allow(PERMISSION_IN_STANDINGS,                   USER_MASK_CONTESTANT);
	allow(PERMISSION_SEE_STANDINGS,                  USER_MASK_NOMARKER);
	allow(PERMISSION_SEE_ALL_STANDINGS,              USER_MASK_OBSERVER | USER_MASK_JUDGE | USER_MASK_ADMIN);
	allow(PERMISSION_SEE_FINAL_STANDINGS,            USER_MASK_JUDGE | USER_MASK_ADMIN);
	allow(PERMISSION_SEE_ALL_CLARIFICATION_REQUESTS, USER_MASK_JUDGE | USER_MASK_ADMIN);
	allow(PERMISSION_SEE_ALL_CLARIFICATIONS,         USER_MASK_JUDGE | USER_MASK_ADMIN);
	allow(PERMISSION_SEE_ALL_SUBMISSIONS,            USER_MASK_JUDGE | USER_MASK_ADMIN);
	allow(PERMISSION_SEE_ALL_PROBLEMS,               USER_MASK_JUDGE | USER_MASK_ADMIN);
	allow(PERMISSION_SEE_PROBLEM_DETAILS,            USER_MASK_JUDGE | USER_MASK_MARKER | USER_MASK_ADMIN);
	allow(PERMISSION_SEE_SUBMISSION_DETAILS,         USER_MASK_JUDGE | USER_MASK_ADMIN);
	allow(PERMISSION_SEE_USER_ID,                    USER_MASK_ADMIN);
	allow(PERMISSION_MARK,                           USER_MASK_MARKER | USER_MASK_ADMIN);
	allow(PERMISSION_JUDGE,                          USER_MASK_JUDGE | USER_MASK_ADMIN);
	allow(PERMISSION_JUDGE_OVERRIDE,                 USER_MASK_ADMIN);
	allow(PERMISSION_USER_ADMIN,                     USER_MASK_ADMIN);
	allow(PERMISSION_SERVER_ADMIN,                   USER_MASK_ADMIN);
	allow(PERMISSION_PROBLEM_ADMIN,                  USER_MASK_ADMIN);
	allow(PERMISSION_START_STOP,                     USER_MASK_ADMIN);
}

Permissions::~Permissions() {
}

Permissions *Permissions::getInstance() {
	static Permissions instance;
	return &instance;
}

bool Permissions::hasPermission(UserType type, const PermissionSet &ps) const {
	switch (ps._type)
	{
	case PermissionSet::EXPRESSION_TYPE_BASE:
		return _perms.count(make_pair(type, ps._base));
	case PermissionSet::EXPRESSION_TYPE_OR:
		return hasPermission(type, *ps._left) || hasPermission(type, *ps._right);
	case PermissionSet::EXPRESSION_TYPE_AND:
		return hasPermission(type, *ps._left) && hasPermission(type, *ps._right);
	case PermissionSet::EXPRESSION_TYPE_NOT:
		return !hasPermission(type, *ps._left);
	}
}

bool Permissions::hasPermission(const ClientConnection *cc, const PermissionSet &ps) const {
	UserType user_type = static_cast<UserType>(cc->getProperty("user_type"));
	return hasPermission(user_type, ps);
}
