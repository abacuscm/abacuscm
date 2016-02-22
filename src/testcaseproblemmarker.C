/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "testcaseproblemmarker.h"
#include "buffer.h"
#include "logger.h"

#include <string>
#include <fstream>
#include <sys/stat.h>

using namespace std;

void TestCaseProblemMarker::mark_compiled() {
	Buffer *in = getProblemFile("testcase.input");

	if(!in) {
		log(LOG_ERR, "Error retrieving desired input file");
		return;
	}

	string infile = workdir() + "/stdin";
	string outfile = workdir() + "/stdout";
	string errfile = workdir() + "/stderr";
	string runfile = workdir() + "/runinfo";
	string corrfile = workdir() + "/correct";
	string difffile = workdir() + "/diff";
	string checkerfile = workdir() + "/checker";

	ofstream file_stdin(infile.c_str());
	file_stdin << *in;
	file_stdin.close();
	delete in;

	if(run(infile.c_str(),
				outfile.c_str(),
				errfile.c_str(),
				runfile.c_str()) == 0) {

		addResultFile("Team's error log", errfile, 64 * 1024);
		// parse the runinfo output
		if (_run_info)
			delete _run_info;
		_run_info = new RunInfo(runfile.c_str());

		if (_run_info->timeExceeded) {
			// exceeded time limit
			// set some appropriate stuff, and return
			// includes adding files to send to judges
			log(LOG_DEBUG, "Program exceeded time limit (time was %u; %s time limit was exceeded)", _run_info->time, _run_info->cpuExceeded ? "cpu" : _run_info->realTimeExceeded ? "real" : "unknown");
			setResult(TIME_EXCEEDED);
			return;
		}
		if (!_run_info->normalTerm || _run_info->exitCode != 0) {
			// bad exit code
			// set some appropriate stuff, and return
			// includes adding files to send to judges
			if (_run_info->signalTerm)
				log(LOG_DEBUG, "Abnormal termination of program (caught signal %d -- %s)", _run_info->signal, _run_info->signalName);
			else
				log(LOG_DEBUG, "Abnormal termination of program (non-zero exit code of %d)", _run_info->exitCode);
			setResult(ABNORMAL);
			return;
		}

		// if we get here, then we have a solution that completed in time,
		// exited normally and had an exit code of 0 == good :-)
		log(LOG_DEBUG, "Normal termination of program (finished in time of %u)", _run_info->time);

		Buffer *out = getProblemFile("testcase.output");

		if(!out) {
			log(LOG_ERR, "Error retrieving the desired output");
			return;
		}

		ofstream file_correct(corrfile.c_str());
		file_correct << *out;
		file_correct.close();
		delete out;

		Buffer *checker = getProblemFile("checker");
		if (checker) {
			ofstream file_checker(checkerfile.c_str());
			file_checker << *checker;
			if (!file_checker) {
				log(LOG_ERR, "failed to write checker");
				return;
			}
			file_checker.close();
			delete checker;
			if (chmod(checkerfile.c_str(), 0700) != 0) {
				lerror("chmod");
				return;
			}
			// TODO: use safe_system
			string checker_cmd = checkerfile + " " + infile + " " + outfile + " " + corrfile + " >" + difffile;
			log(LOG_DEBUG, "Using checker command '%s'", checker_cmd.c_str());

			int excode = system(checker_cmd.c_str());
			if (excode != 0) {
				setResult(OTHER);
				addResultFile("Team's output", outfile, 64 * 1024);
				log(LOG_ERR, "Checker returned exit code %d", excode);
			}
			else {
				RunResult status;
				string info;
				ifstream out(difffile.c_str());
				parse_checker(out, status, info);
				setResult(status);
				out.close();
				addResultFile("Team's output", outfile, 64 * 1024);
				addResultFile("Checker log", difffile, 64 * 1024);
			}
		}
		else {
			string diff_cmd = "/usr/bin/diff -U 10000000 ";
			if(attrib("ignore_whitespace") == "Yes")
				diff_cmd += "-B -b ";
			diff_cmd += outfile + " " + corrfile + " >" + difffile + " 2>&1";
			log(LOG_DEBUG, "Using diff command '%s'", diff_cmd.c_str());

			int excode = system(diff_cmd.c_str());
			if(excode == 0) {
				setResult(CORRECT);
				log(LOG_DEBUG, "Correct!");
			} else {
				setResult(JUDGE);
				addResultFile("Team's output", outfile, 64 * 1024);
				addResultFile("Diff of team's output versus expected output", difffile, 64 * 1024);
				log(LOG_DEBUG, "Incorrect!");
			}
		}
	} else {
		log(LOG_ERR, "An error has occured whilst running the user program.");
	}
}

static ProblemMarker* TestCaseProblemMarkerFunctor() {
	return new TestCaseProblemMarker();
}

static __attribute__((constructor)) void init() {
	ProblemMarker::registerMarker("tcprob", TestCaseProblemMarkerFunctor);
}
