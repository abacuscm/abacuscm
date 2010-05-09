/**
 * Copyright (c) 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id: testcaseproblemmarker.h 448 2007-08-06 10:25:29Z bmerry $
 */
#ifndef __INTERACTIVEPROBLEMMARKER_H__
#define __INTERACTIVEPROBLEMMARKER_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "compiledproblemmarker.h"

class InteractiveProblemMarker : public CompiledProblemMarker {
protected:
	virtual void mark_compiled();
};

#endif
