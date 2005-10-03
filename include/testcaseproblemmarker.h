#ifndef __TESTCASEPROBLEMMARKER_H__
#define __TESTCASEPROBLEMMARKER_H__

#include "compiledproblemmarker.h"

class TestCaseProblemMarker : public CompiledProblemMarker {
protected:
	virtual void mark_compiled();
};

#endif
