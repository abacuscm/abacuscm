/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "buffer.h"
#include "problemmarker.h"
#include "logger.h"
#include "acmconfig.h"
#include "serverconnection.h"
#include "queue.h"
#include "messageblock.h"

#include <iostream>

using namespace std;

static Queue<MarkRequest*> _mark_requests;

static void mark_request(const MessageBlock* mb, void*) {
	MarkRequest *mr = new MarkRequest;
	mr->prob_id = strtoll((*mb)["prob_id"].c_str(), NULL, 0);
	mr->submission_id = strtoll((*mb)["submission_id"].c_str(), NULL, 0);
	mr->lang = (*mb)["language"];
	mr->submission.appendData(mb->content(), mb->content_size());
	_mark_requests.enqueue(mr);
}

static void server_close(const MessageBlock*, void*) {
	_mark_requests.enqueue(NULL);
}

int main(int argc, char **argv) {
	Config &conf = Config::getConfig();
	conf.load(DEFAULT_MARKER_CONFIG);
	if(argc > 1)
		conf.load(argv[1]);

	log(LOG_DEBUG, "Loaded, proceeding to mark some code ...");

	for (;;) {
		ServerConnection _server_con;
		if(!_server_con.connect(conf["server"]["address"], conf["server"]["service"]))
			goto retry;

		if(!_server_con.auth(conf["server"]["username"], conf["server"]["password"])) {
			// Perhaps the server hasn't been populated with the correct user credentials
			// yet. Let's sleep and then have another go...
			goto disconnect;
		}

		log(LOG_DEBUG, "Connected to server!");

		_server_con.registerEventCallback("mark", mark_request, NULL);
		_server_con.registerEventCallback("close", server_close, NULL);

		if(!_server_con.becomeMarker()) {
			// More severe than failing to login, but again perhaps there's just a glitch
			// on the server with our user setup. Sleep and have another go...
			log(LOG_DEBUG, "Failed to become a marker");
			goto disconnect;
		}

		MarkRequest *mr;
		while((mr = _mark_requests.dequeue()) != NULL) {
			AttributeMap attrs;
			if(!_server_con.getProblemAttributes(mr->prob_id, attrs)) {
				log(LOG_CRIT, "Cannot function if we cannot retrieve problem attributes!");
				goto disconnect;
			}

			string problem_type = attrs["prob_type"];
			ProblemMarker* marker = ProblemMarker::createMarker(problem_type);

			if (!marker) {
				log(LOG_CRIT, "Error locating marker for problem type '%s'.", attrs["prob_type"].c_str());
				goto disconnect;
			}

			marker->setServerCon(&_server_con);
			marker->setMarkRequest(mr);
			marker->setProblemAttributes(attrs);

			marker->mark();
			if(!marker->submitResult()) {
				log(LOG_CRIT, "Great, failed to upload mark, bailing entirely ...");
				goto disconnect;
			}

			delete marker;
		}

		log(LOG_INFO, "Server has disconnected");

		disconnect:;
		_server_con.disconnect();

		retry:;
		log(LOG_INFO, "Sleeping for 10 seconds");
		sleep(10);
	}

	return 0;
}
