/**
 * Copyright (c) 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "interactiveproblemmarker.h"
#include "buffer.h"
#include "logger.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>

using namespace std;

void InteractiveProblemMarker::mark_compiled() {
	Buffer *in = getProblemFile("evaluator");

	if (!in) {
		log(LOG_ERR, "Error retrieving evaluator");
		return;
	}

	string outfile = workdir() + "/stdout";
	string errfile = workdir() + "/stderr";
	string runfile = workdir() + "/runinfo";

	string evaluatorfile = workdir() + "/evaluator";
	ofstream file_evaluator(evaluatorfile.c_str());
	file_evaluator << *in;
	if (!file_evaluator) {
		log(LOG_ERR, "failed to write evaluator");
		delete in;
		return;
	}
	file_evaluator.close();
	delete in;

	if (chmod(evaluatorfile.c_str(), 0700) != 0) {
		lerror("chmod");
		return;
	}

	if (run("/dev/null", outfile.c_str(), errfile.c_str(), runfile.c_str(), evaluatorfile.c_str()) == 0) {
		string status_str = "";
		RunResult status = OTHER;
		string info = "";
		ifstream out(outfile.c_str());
		parse_checker(out, status, info);

		setResult(status);
		addResultFile("Evaluation log", outfile, 64 * 1024);
		addResultFile("Runinfo", runfile, 64 * 1024);
		addResultFile("Team's error log", errfile, 64 * 1024);
		log(LOG_DEBUG, "%s", info.c_str());
	}
	else {
		log(LOG_ERR, "An error has occurred whilst running the user program.");
		setResult(OTHER);
		addResultFile("Evaluation log", outfile, 64 * 1024);
		addResultFile("Runinfo", runfile, 64 * 1024);
		addResultFile("Team's error log", errfile, 64 * 1024);
	}
}

static ProblemMarker* InteractiveProblemMarkerFunctor() {
	return new InteractiveProblemMarker();
}

static __attribute__((constructor)) void init() {
	ProblemMarker::registerMarker("interactiveprob", InteractiveProblemMarkerFunctor);
}
