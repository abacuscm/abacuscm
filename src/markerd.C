#include "buffer.h"
#include "problemmarker.h"
#include "logger.h"
#include "config.h"
#include "sigsegv.h"
#include "serverconnection.h"
#include "queue.h"
#include "messageblock.h"

#include <iostream>

using namespace std;

static ServerConnection _server_con;
static Queue<MarkRequest*> _mark_requests;

static void mark_request(const MessageBlock* mb, void*) {
	MarkRequest *mr = new MarkRequest;
	mr->prob_id = strtoll((*mb)["prob_id"].c_str(), NULL, 0);
	mr->submission_id = strtoll((*mb)["submission_id"].c_str(), NULL, 0);
	mr->lang = (*mb)["language"];
	mr->submission.appendData(mb->content(), mb->content_size());
	_mark_requests.enqueue(mr);
}

int main(int argc, char **argv) {
	setup_sigsegv();
	Config &conf = Config::getConfig();
	conf.load("/etc/abacus/marker.conf");
	if(argc > 1)
		conf.load(argv[1]);

	log(LOG_DEBUG, "Loaded, proceeding to mark some code ...");

	if(!_server_con.connect(conf["server"]["address"], conf["server"]["service"]))
		return -1;

	if(!_server_con.auth(conf["server"]["username"], conf["server"]["password"]))
		return -1;

	log(LOG_DEBUG, "Connected to server!");
	
	_server_con.registerEventCallback("mark", mark_request, NULL);
	if(!_server_con.becomeMarker())
		return -1;

	MarkRequest *mr;
	while((mr = _mark_requests.dequeue()) != NULL) {
		AttributeMap attrs;
		if(!_server_con.getProblemAttributes(mr->prob_id, attrs)) {
			log(LOG_CRIT, "Cannot function if we cannot retrieve problem attributes!");
			return -1;
		}

		string problem_type = attrs["prob_type"];
		ProblemMarker* marker = ProblemMarker::createMarker(problem_type);

		marker->setServerCon(&_server_con);
		marker->setMarkRequest(mr);
		marker->setProblemAttributes(attrs);

		marker->mark();
		if(!marker->submitResult()) {
			log(LOG_CRIT, "Great, failed to upload mark, bailing entirely ...");
			return -1;
		}

		delete marker;
	}

	log(LOG_INFO, "Terminating");

	return 0;
	
	// while(true)
		// dequeue mark request
		// obtain correct ProblemMarker object
		// mark request
		// give feedback
		//
/*	
	ProblemMarker* marker = ProblemMarker::createMarker("tcprob", 1);
	if(!marker) {
		log(LOG_ERR, "Unable to locate the marker for 'tcprob'");
	} else {
		//std::string submission_str = "int main() { printf(\"Hello world!\\n\"); return 0; }\n";
		std::string submission_str = "class Hello { public static void main(String[] args) { System.out.println(\"Hello World!\"); } }";

		Buffer submission;	
		submission.appendData(submission_str.c_str(), submission_str.length());
		
		marker->mark(submission, "Java");
		delete marker;
	}
*/
}
