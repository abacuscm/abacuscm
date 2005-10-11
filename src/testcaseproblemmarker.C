#include "testcaseproblemmarker.h"
#include "buffer.h"
#include "logger.h"

#include <string>
#include <fstream>

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

	ofstream file_stdin(infile.c_str());
	file_stdin << *in;
	file_stdin.close();
	delete in;
	
	if(run(infile.c_str(),
			outfile.c_str(),
			errfile.c_str(),
			runfile.c_str()) == 0) {
		Buffer *out = getProblemFile("testcase.output");

		if(!out) {
			log(LOG_ERR, "Error retrieving the desired output");
			return;
		}

		ofstream file_correct(corrfile.c_str());
		file_correct << *out;
		file_correct.close();
		delete out;

		string diff_cmd = "/usr/bin/diff -U 10000000 ";
		if(attrib("ignore_whitespace") == "Yes")
			diff_cmd += "-B -b ";
		diff_cmd += outfile + " " + corrfile + " &>" + difffile;
		log(LOG_DEBUG, "Using diff command '%s'", diff_cmd.c_str());

		int excode = system(diff_cmd.c_str());
		if(excode == 0) {
			NOT_IMPLEMENTED();
			log(LOG_DEBUG, "Correct!");
		} else {
			NOT_IMPLEMENTED();
			log(LOG_DEBUG, "Incorrect!");
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
