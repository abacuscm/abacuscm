/**
 * Copyright (c) 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */

#ifndef __PERMISSIONS_H
#define __PERMISSIONS_H

#include <stdint.h>
#include <string>
#include <vector>
#include "misc.h"

class ClientConnection;

/* Note: if you change this enumeration, be sure to update
 * - permission_names in src/permissions.C
 * - the list of grants in src/permissionmap.C
 */
typedef enum {
	PERMISSION_INVALID = -1,                      // Not a real permission
	PERMISSION_AUTH = 0,                          // Successfully logged in
	PERMISSION_SUBMIT,                            // Submit a solution
	PERMISSION_CLARIFICATION_REQUEST,             // Ask for clarification
	PERMISSION_CLARIFICATION_REPLY,               // Provide a clarification
	PERMISSION_CHANGE_PASSWORD,                   // Set own password
	PERMISSION_IN_STANDINGS,                      // Show user in standings
	PERMISSION_SEE_STANDINGS,                     // See any standings at all
	PERMISSION_SEE_ALL_STANDINGS,                 // See users without PERMISSION_IN_STANDINGS in standings
	PERMISSION_SEE_FINAL_STANDINGS,               // See standings updates from last hour
	PERMISSION_SEE_ALL_CLARIFICATION_REQUESTS,    // See other user's requests
	PERMISSION_SEE_ALL_CLARIFICATIONS,            // See replies to all clarifications
	PERMISSION_SEE_ALL_SUBMISSIONS,               // See other user's submissions
	PERMISSION_SEE_ALL_PROBLEMS,                  // All problems are submissible
	PERMISSION_SEE_PROBLEM_DETAILS,               // See secret information about problems (e.g. input)
	PERMISSION_SEE_SUBMISSION_DETAILS,            // See output, expected output etc.
	PERMISSION_SEE_USER_ID,                       // See the team for clarification requests and submissions
	PERMISSION_MARK,                              // Assign mark to work assigned for marking
	PERMISSION_JUDGE,                             // Assign right or wrong when in Deferred to Judge state
	PERMISSION_JUDGE_OVERRIDE,                    // Allow any mark to be assigned, overriding others
	PERMISSION_SET_BONUS,                         // Set a time or points bonus for a user
	PERMISSION_USER_ADMIN,                        // Create or modify users
	PERMISSION_PROBLEM_ADMIN,                     // Create or modify problems
	PERMISSION_SERVER_ADMIN,                      // Create or modify slave servers
	PERMISSION_START_STOP                         // Control contest state
} Permission;

const size_t PERMISSION_COUNT = static_cast<uint32_t>(PERMISSION_START_STOP) + 1;

const char *getPermissionName(Permission p);

// Maps a name back to the enum. Returns PERMISSION_INVALID if not found.
Permission getPermissionValue(const std::string &name);

class PermissionSet : public std::vector<bool> {
public:
	PermissionSet() : std::vector<bool>(PERMISSION_COUNT) {}
};

// A predicate on a permission set, built
// from AND, OR and NOT conditions.
class PermissionTest
{
private:
	enum ExpressionType
	{
		EXPRESSION_TYPE_NONE,
		EXPRESSION_TYPE_ANY,
		EXPRESSION_TYPE_BASE,
		EXPRESSION_TYPE_OR,
		EXPRESSION_TYPE_AND,
		EXPRESSION_TYPE_NOT
	};

	ExpressionType  _type;
	Permission      _base;
	PermissionTest *_left;
	PermissionTest *_right;

	explicit PermissionTest(ExpressionType);
public:
	static const PermissionTest ANY;
	static const PermissionTest NONE;

	PermissionTest();             // Never matches
	PermissionTest(Permission);   // Matches the permission only
	PermissionTest(const PermissionTest &pt);

	~PermissionTest();

	PermissionTest& operator=(const PermissionTest &pt);

	PermissionTest operator ||(const PermissionTest &pt) const;
	PermissionTest operator &&(const PermissionTest &pt) const;
	PermissionTest operator !() const;

	bool matches(const PermissionSet &perms) const;
};

static inline PermissionTest operator ||(Permission perm1, Permission perm2)
{
	return PermissionTest(perm1) || PermissionTest(perm2);
}

static inline PermissionTest operator &&(Permission perm1, Permission perm2)
{
	return PermissionTest(perm1) && PermissionTest(perm2);
}

static inline PermissionTest operator !(Permission perm)
{
	return !PermissionTest(perm);
}

#endif
