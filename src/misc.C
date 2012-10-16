/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "misc.h"

const char * const runMessages[OTHER + 1] = {
	"Correct answer",
	"Wrong answer",
	"Time limit exceeded",
	"Abnormal termination of program",
	"Compilation failed",
	"Deferred to judge",
	"Format error",
	"Other - contact a judge"
};

const char * const runCodes[OTHER + 1] = {
	"CORRECT",
	"WRONG",
	"TIME_EXCEEDED",
	"ABNORMAL",
	"COMPILE_FAILED",
	"JUDGE",
	"FORMAT_ERROR",
	"OTHER"
};
