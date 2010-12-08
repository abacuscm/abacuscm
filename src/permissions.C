/**
 * Copyright (c) 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */

#include "permissions.h"
#include "misc.h"
#include <cassert>

using namespace std;

static const char * const _permission_names[PERMISSION_COUNT] = {
	"auth",
	"submit",
	"clarification_request",
	"clarification_reply",
	"change_password",
	"in_standings",
	"see_standings",
	"see_all_standings",
	"see_final_standings",
	"see_all_clarification_requests",
	"see_all_clarifications",
	"see_all_submissions",
	"see_all_problems",
	"see_problem_details",
	"see_submission_details",
	"see_user_id",
	"mark",
	"judge",
	"judge_override",
	"user_admin",
	"problem_admin",
	"server_admin",
	"start_stop"
};

const char * getPermissionName(Permission perm) {
	assert(static_cast<size_t>(perm) < PERMISSION_COUNT);
	return _permission_names[perm];
}

Permission getPermissionValue(const string &name) {
	for (size_t i = 0; i < PERMISSION_COUNT; i++) {
		if (name == _permission_names[i]) {
			return static_cast<Permission>(i);
		}
	}
	return PERMISSION_INVALID;
}

PermissionTest::PermissionTest()
: _type(EXPRESSION_TYPE_NONE), _base(PERMISSION_INVALID), _left(NULL), _right(NULL) {
}

PermissionTest::PermissionTest(Permission perm)
: _type(EXPRESSION_TYPE_BASE), _base(perm), _left(NULL), _right(NULL) {
}

PermissionTest::PermissionTest(ExpressionType type)
: _type(type), _base(PERMISSION_INVALID), _left(NULL), _right(NULL) {
	assert(type == EXPRESSION_TYPE_ANY || type == EXPRESSION_TYPE_NONE);
}

const PermissionTest PermissionTest::NONE(EXPRESSION_TYPE_NONE);
const PermissionTest PermissionTest::ANY(EXPRESSION_TYPE_ANY);

PermissionTest::PermissionTest(const PermissionTest &pt)
: _type(pt._type), _base(pt._base), _left(NULL), _right(NULL) {
	if (pt._left != NULL)
		_left = new PermissionTest(*pt._left);
	if (pt._right != NULL)
		_right = new PermissionTest(*pt._right);
}

PermissionTest::~PermissionTest() {
	delete _left;
	delete _right;
}

PermissionTest &PermissionTest::operator=(const PermissionTest &pt) {
	delete _left; _left = NULL;
	delete _right; _right = NULL;

	_type = pt._type;
	_base = pt._base;
	if (pt._left != NULL)
		_left = new PermissionTest(*pt._left);
	if (pt._right != NULL)
		_right = new PermissionTest(*pt._right);
	return *this;
}

PermissionTest PermissionTest::operator ||(const PermissionTest &pt) const {
	PermissionTest out;
	out._type = EXPRESSION_TYPE_OR;
	out._left = new PermissionTest(*this);
	out._right = new PermissionTest(pt);
	return out;
}

PermissionTest PermissionTest::operator &&(const PermissionTest &pt) const {
	PermissionTest out;
	out._type = EXPRESSION_TYPE_AND;
	out._left = new PermissionTest(*this);
	out._right = new PermissionTest(pt);
	return out;
}

PermissionTest PermissionTest::operator !() const {
	PermissionTest out;
	out._type = EXPRESSION_TYPE_NOT;
	out._left = new PermissionTest(*this);
	return out;
}

bool PermissionTest::matches(const PermissionSet &ps) const {
	switch (_type)
	{
	case PermissionTest::EXPRESSION_TYPE_BASE:
		return ps[_base];
	case PermissionTest::EXPRESSION_TYPE_OR:
		return _left->matches(ps) || _right->matches(ps);
	case PermissionTest::EXPRESSION_TYPE_AND:
		return _left->matches(ps) && _right->matches(ps);
	case PermissionTest::EXPRESSION_TYPE_NOT:
		return !_left->matches(ps);
	case PermissionTest::EXPRESSION_TYPE_ANY:
		return true;
	case PermissionTest::EXPRESSION_TYPE_NONE:
		return false;
	}
	assert(0);
	return false;
}
