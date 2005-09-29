#include "problemmarker.h"

using namespace std;

ProblemMarker::FunctorMap ProblemMarker::_functors
		__attribute__((init_priority(101)));

ProblemMarker::ProblemMarker() {
}

ProblemMarker::~ProblemMarker() {
}

		
ProblemMarker* ProblemMarker::createMarker(string problemtype, uint32_t prob_id) {
	if(!_functors[problemtype])
		return NULL;
	ProblemMarker *marker = _functors[problemtype]();

	if(!marker)
		return NULL;

	marker->_attribs["testcase.input"] = "hello.in";
	marker->_attribs["testcase.output"] = "hello.out";
	marker->_attribs["time_limit"] = 120;
	marker->_attribs["ignore_whitespace"] = "No";
	marker->_attribs["longname"] = "Hello World!";
	marker->_attribs["shortname"] = "hi";

	marker->_prob_id = prob_id;

	return marker;
}

void ProblemMarker::registerMarker(std::string problemtype,
		ProblemMarkerFunctor func) {
	_functors[problemtype] = func;
}
