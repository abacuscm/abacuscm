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
#include <set>
#include <utility>
#include <memory>
#include "misc.h"

class ClientConnection;

typedef enum {
	PERMISSION_NONE,                              // No user has this
	PERMISSION_ANY,                               // All users have this
	PERMISSION_AUTH,                              // Users have this once authenticated
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
	PERMISSION_USER_ADMIN,                        // Create or modify users
	PERMISSION_PROBLEM_ADMIN,                     // Create or modify problems
	PERMISSION_SERVER_ADMIN,                      // Create or modify slave servers
	PERMISSION_START_STOP                         // Control contest state
} Permission;

// A predicate on a permission set, built
// from AND, OR and NOT conditions.
class PermissionSet
{
private:
	enum ExpressionType
	{
		EXPRESSION_TYPE_BASE,
		EXPRESSION_TYPE_OR,
		EXPRESSION_TYPE_AND,
		EXPRESSION_TYPE_NOT
	};

	ExpressionType _type;
	Permission     _base;
	PermissionSet *_left;
	PermissionSet *_right;

	friend class Permissions;

public:
	PermissionSet();             // Never matches
	PermissionSet(Permission);   // Matches the permission only
	PermissionSet(const PermissionSet &ps);

	~PermissionSet();

	PermissionSet& operator=(const PermissionSet &ps);

	PermissionSet operator ||(const PermissionSet &ps) const;
	PermissionSet operator &&(const PermissionSet &ps) const;
	PermissionSet operator !() const;
};

static inline PermissionSet operator ||(Permission perm1, Permission perm2)
{
	return PermissionSet(perm1) || PermissionSet(perm2);
}

static inline PermissionSet operator &&(Permission perm1, Permission perm2)
{
	return PermissionSet(perm1) && PermissionSet(perm2);
}

static inline PermissionSet operator !(Permission perm)
{
	return !PermissionSet(perm);
}

/* Manages permission mappings */
class Permissions {
private:
	Permissions();
	~Permissions();

	std::set<std::pair<UserType, Permission> > _perms;

	// Adds permission to the set. The mask is an OR of
	// USER_MASK_* constants.
	void allow(Permission perm, uint32_t mask);
public:
	static Permissions *getInstance();

	bool hasPermission(UserType user_type, const PermissionSet &ps) const;
	bool hasPermission(const ClientConnection *cc, const PermissionSet &ps) const;
};

#endif
