#include "testcaseproblemmarker.h"
#include "buffer.h"

void TestCaseProblemMarker::mark_compiled() {
	Buffer in, out, err;
	in.appendData("Hello World Input\n", strlen("Hello World Input\n"));
	run(in, out, err);
}

static ProblemMarker* TestCaseProblemMarkerFunctor() {
	return new TestCaseProblemMarker();
}

static __attribute__((constructor)) void init() {
	ProblemMarker::registerMarker("tcprob", TestCaseProblemMarkerFunctor);
}
