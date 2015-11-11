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

typedef enum {
	PENDING = -1,
	CORRECT = 0,
	WRONG = 1,
	TIME_EXCEEDED = 2,
	ABNORMAL = 3,
	COMPILE_FAILED = 4,
	JUDGE = 5,
	FORMAT_ERROR = 6,
	OTHER = 7
} RunResult;

typedef enum {
	USER_TYPE_NONE = 0,
	USER_TYPE_ADMIN = 1,
	USER_TYPE_JUDGE = 2,
	USER_TYPE_CONTESTANT = 3,
	USER_TYPE_MARKER = 4,
	USER_TYPE_OBSERVER = 5,
	USER_TYPE_PROCTOR = 6
} UserType;

/* Column numbering for standings client-server messages. Note: if you
 * update this, be sure to update the corresponding enumerations in
 * standings.pl and standings.js.
 */
typedef enum {
	STANDING_RAW_ID = 0,
	STANDING_RAW_USERNAME = 1,
	STANDING_RAW_FRIENDLYNAME = 2,
	STANDING_RAW_GROUP = 3,
	STANDING_RAW_CONTESTANT = 4,
	STANDING_RAW_TOTAL_SOLVED = 5,
	STANDING_RAW_TOTAL_TIME = 6,
	STANDING_RAW_SOLVED = 7
} StandingColumnRaw;

static const unsigned int USER_MASK_NONE = 1 << USER_TYPE_NONE;
static const unsigned int USER_MASK_ADMIN = 1 << USER_TYPE_ADMIN;
static const unsigned int USER_MASK_JUDGE = 1 << USER_TYPE_JUDGE;
static const unsigned int USER_MASK_CONTESTANT = 1 << USER_TYPE_CONTESTANT;
static const unsigned int USER_MASK_MARKER = 1 << USER_TYPE_MARKER;
static const unsigned int USER_MASK_OBSERVER = 1 << USER_TYPE_OBSERVER;
static const unsigned int USER_MASK_PROCTOR = 1 << USER_TYPE_PROCTOR;

// All logged in types
static const unsigned int USER_MASK_ALL = USER_MASK_ADMIN | USER_MASK_JUDGE | USER_MASK_CONTESTANT | USER_MASK_MARKER | USER_MASK_OBSERVER | USER_MASK_PROCTOR;
// All types except a marker
static const unsigned int USER_MASK_NOMARKER = USER_MASK_ALL & ~USER_MASK_MARKER;
// All types that are involved in running the contest
static const unsigned int USER_MASK_MANAGER = USER_MASK_JUDGE | USER_MASK_ADMIN;

extern const char * const runMessages[OTHER + 1];
extern const char * const runCodes[OTHER + 1];

#define NULL_TIME ((time_t) -1)

#endif
