#include "problemmarker.h"
#include "logger.h"
#include "config.h"

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

string ProblemMarker::workdir() {
	if(_workdir != "")
		return _workdir;

	string templ = Config::getConfig()["marker"]["workdir"];
	if(templ == "") {
		templ = "/tmp/abacus-marker-XXXXXX";
	}

	char *ch_templ = strdup(templ.c_str());

	char *workdir = mkdtemp(ch_templ);
	if(!workdir) {
		lerror("mkdtemp");
	} else {
		log(LOG_INFO, "Using working directory '%s'", ch_templ);
		_workdir = workdir;
	}
	free(ch_templ);

	return _workdir;
}
