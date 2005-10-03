#include "buffer.h"
#include "problemmarker.h"
#include "logger.h"
#include "config.h"

#include <iostream>

using namespace std;

int main(int argc, char **argv) {
	Config &conf = Config::getConfig();
	conf.load("/etc/abacus/marker.conf");
	if(argc > 1)
		conf.load(argv[1]);

	log(LOG_DEBUG, "Loaded, proceeding to mark some code ...");
	// connect to server
	
	// while(true)
		// dequeue mark request
		// obtain correct ProblemMarker object
		// mark request
		// give feedback
		//
	
	ProblemMarker* marker = ProblemMarker::createMarker("tcprob", 1);
	if(!marker) {
		log(LOG_ERR, "Unable to locate the marker for 'tcprob'");
	} else {
		std::string submission_str = "int main() { printf(\"Hello world!\\n\"); return 0; }\n";

		Buffer submission;	
		submission.appendData(submission_str.c_str(), submission_str.length());
		
		marker->mark(submission, "C");
	}
}
