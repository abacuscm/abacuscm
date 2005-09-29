#include "buffer.h"
#include "problemmarker.h"

#include <iostream>

int main() {
	// load config

	// connect to server
	
	// while(true)
		// dequeue mark request
		// obtain correct ProblemMarker object
		// mark request
		// give feedback
		//
	
	ProblemMarker* marker = ProblemMarker::createMarker("tcprob", 1);
	if(!marker) {
		std::cerr << "SHIT!\n";
	} else {
		std::string submission_str = "int main() { printf(\"Hello world!\\n\"";

		Buffer submission;	
		submission.appendData(submission_str.c_str(), submission_str.length());
		
		marker->mark(submission, "lang");
	}
}
