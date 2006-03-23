/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __TESTCASEPROBLEMMARKER_H__
#define __TESTCASEPROBLEMMARKER_H__

#include "compiledproblemmarker.h"

class TestCaseProblemMarker : public CompiledProblemMarker {
protected:
	virtual void mark_compiled();
};

#endif
