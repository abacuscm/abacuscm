#include "testcaseproblemmarker.h"

static ProblemMarker* TestCaseProblemMarkerFunctor() {
	return new TestCaseProblemMarker();
}

__attribute__((constructor))
static void init() {
	ProblemMarker::registerMarker("tcprob", TestCaseProblemMarkerFunctor);
}
