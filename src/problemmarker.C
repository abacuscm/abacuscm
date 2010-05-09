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
#include "acmconfig.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
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
	if(_workdir != "")
		cleanWorkdir(_workdir);

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
		if (stat(filename.c_str(), &info) == 0 && info.st_size > max_size)
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

/* Similar to system(), but bypasses the shell (more like exec)
 * It also reports any non-zero exit status
 */
static int safe_system(const char *filename, const char * const * argv) {
	pid_t pid;
	int status = -1;

	pid = fork();
	if (pid == -1) {
		lerror("fork");
		return -1;
	} else if (pid == 0) {
		/* Child */
		execv(filename, (char * const *) argv);
		_exit(127); /* Only get here on error */
	} else {
		/* Parent */
		pid_t result;
		result = waitpid(pid, &status, 0);
		if (result == -1)
			lerror("waitpid");

		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) != 0) {
				log(LOG_WARNING, "%s exited with status %d", filename, WEXITSTATUS(status));
			}
		} else if (WIFSIGNALED(status)) {
			log(LOG_WARNING, "%s exited with signal %d", filename, WTERMSIG(status));
		} else {
			log(LOG_WARNING, "%s returned exit code %d", filename, status);
		}
	}
	return status;
}

void ProblemMarker::cleanWorkdir(const string &wdir) {
	/* If the user has taken away all permissions on a subdirectory, rm -rf
	 * will refuse to remove it. Force everything to be writable first.
	 */
	const char * const chmod_argv[] = {"/bin/chmod", "u+rwX", "--", wdir.c_str(), NULL};
	const char * const rm_argv[] = {"/bin/rm", "-rf", "--", wdir.c_str(), NULL};
	safe_system("/bin/chmod", chmod_argv);
	safe_system("/bin/rm", rm_argv);
}
