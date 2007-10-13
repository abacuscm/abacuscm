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
	OTHER = 6
} RunResult;

extern char *runMessages[];
extern char *shortRunMessages[];

#endif
