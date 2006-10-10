/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "problemmarker.h"
#include "logger.h"
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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
	if(_run_info)
		delete _run_info;

	// leave _server_con alone.
}

bool ProblemMarker::submitResult() {
	if(!_have_result)
		return false;

	return _server_con->mark(_mr->submission_id, _result, runMessages[_result], _result_files);
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

// The final argument may be non-zero to specify a size to truncate the
// file to.
void ProblemMarker::addResultFile(std::string fileType, std::string filename, off_t max_size) {
	_result_files[fileType] = filename;
	if (max_size)
	{
		struct stat info;
		if (stat(filename.c_str(), &info) && info.st_size > max_size)
			truncate(filename.c_str(), max_size);
	}
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
		chmod(workdir, 0711);
		_workdir = workdir;
	}
	free(ch_templ);


	log(LOG_INFO, "Using working directory: %s", _workdir.c_str());

	return _workdir;
}
