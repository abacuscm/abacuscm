#include "problemmarker.h"
#include "logger.h"
#include "config.h"

using namespace std;

ProblemMarker::FunctorMap ProblemMarker::_functors
		__attribute__((init_priority(101)));

ProblemMarker::ProblemMarker() {
	_mr = NULL;
    _server_con = NULL;
    _run_info = NULL;

    _have_result = false;
}

ProblemMarker::~ProblemMarker() {
	if(_mr)
		delete _mr;
	// leave _server_con alone.
}

void ProblemMarker::setServerCon(ServerConnection *server_con) {
	_server_con = server_con;
}

void ProblemMarker::setMarkRequest(MarkRequest *mr) {
	_mr = mr;
}

void ProblemMarker::setProblemAttributes(AttributeMap& attrs) {
	_attribs = attrs;
}

void ProblemMarker::setResult(RunResult result) {
    _result = result;
    _have_result = true;
}

void ProblemMarker::addResultFile(std::string fileType, std::string filename) {
    _result_files[fileType] = filename;
}

const Buffer& ProblemMarker::submission() {
	return _mr->submission;
}

std::string ProblemMarker::language() {
	return _mr->lang;
}

ProblemMarker* ProblemMarker::createMarker(string problemtype) {
	if(!_functors[problemtype])
		return NULL;
	ProblemMarker *marker = _functors[problemtype]();

	return marker;
}

void ProblemMarker::registerMarker(std::string problemtype,
		ProblemMarkerFunctor func) {
	_functors[problemtype] = func;
}

string ProblemMarker::attrib(string attrname) {
	AttributeMap::iterator i = _attribs.find(attrname);
	if(i == _attribs.end()) {
		log(LOG_WARNING, "Marking system requested attribute '%s' which does not exists", attrname.c_str());
		return "";
	}
	return i->second;
}

Buffer* ProblemMarker::getProblemFile(string attrib) {
	char* bufferptr;
	uint32_t bufferlen;

	if(!_server_con->getProblemFile(_mr->prob_id, attrib, &bufferptr, &bufferlen))
		return NULL;

	Buffer *bfr = new Buffer;
	bfr->appendData(bufferptr, bufferlen);

	delete []bufferptr;
	return bfr;
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

	log(LOG_INFO, "Using working directory: %s", _workdir.c_str());

	return _workdir;
}
