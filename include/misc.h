/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __MISC_H__
#define __MISC_H__

#include <list>

template<typename _Type>
typename std::list<_Type>::iterator list_find(std::list<_Type>& l, const _Type& w) {
	typename std::list<_Type>::iterator i;
	for(i = l.begin(); i != l.end(); ++i)
		if(*i == w)
			return i;
	return l.end();
}

typedef enum {
	PENDING = -1,
	CORRECT = 0,
	WRONG = 1,
	TIME_EXCEEDED = 2,
	ABNORMAL = 3,
	COMPILE_FAILED = 4,
	JUDGE = 5,
	OTHER = 6,
} RunResult;

typedef enum {
	USER_TYPE_NONE = 0,
	USER_TYPE_ADMIN = 1,
	USER_TYPE_JUDGE = 2,
	USER_TYPE_CONTESTANT = 3,
	USER_TYPE_MARKER = 4,
	USER_TYPE_NONCONTEST = 5,
} UserType;

static const int USER_MASK_ADMIN = 1 << USER_TYPE_ADMIN;
static const int USER_MASK_JUDGE = 1 << USER_TYPE_JUDGE;
static const int USER_MASK_CONTESTANT = 1 << USER_TYPE_CONTESTANT;
static const int USER_MASK_MARKER = 1 << USER_TYPE_MARKER;
static const int USER_MASK_NONCONTEST = 1 << USER_TYPE_NONCONTEST;
static const int USER_MASK_ALL = USER_MASK_ADMIN | USER_MASK_JUDGE | USER_MASK_CONTESTANT | USER_MASK_MARKER | USER_MASK_NONCONTEST;

extern char *runMessages[];
extern char *shortRunMessages[];

#endif
