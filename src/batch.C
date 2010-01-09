/**
 * Copyright (c) 2009 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "logger.h"
#include "acmconfig.h"
#include "serverconnection.h"
#include "messageblock.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <stdexcept>
#include <cassert>

using namespace std;

void batch_sendMB(ServerConnection &_server_con, MessageBlock *mb) {
	if (mb == NULL)
		return;

	log(LOG_DEBUG, "Sending %s", mb->action().c_str());
	MessageBlock *ret = _server_con.sendMB(mb);
	if (ret == NULL) {
		log(LOG_WARNING, "Did not receive a response to");
		mb->dump();
		return;
	}

	log(LOG_DEBUG, "Received reply");
	ret->dump();
	delete ret;
}

static int process(ServerConnection &_server_con, istream &in) {
	string line;
	MessageBlock *cur = NULL;
	int lineno = 0;
	while (getline(in, line)) {
		lineno++;
		if (line == "") {
			/* Useful in interactive mode for flushing a record */
			batch_sendMB(_server_con, cur);
			delete cur;
			cur = NULL;
			continue;
		}

		string::size_type split = line.find(':');
		if (split == string::npos) {
			/* Start of a new record */
			batch_sendMB(_server_con, cur);
			delete cur;
			cur = new MessageBlock(line);
		}
		else {
			/* Continuation */
			if (cur == NULL) {
				cerr << "Message data with no message header\n";
				return -1;
			}
			(*cur)[line.substr(0, split)] = line.substr(split + 1);
		}
	}
	batch_sendMB(_server_con, cur);
	delete cur;
	cur = NULL;
	return 0;
}

int main(int argc, const char **argv) {
	if (argc < 2 || argc > 3) {
		cerr << "Usage: " << argv[0] << " config [messagefile]\n";
		return -1;
	}

	ServerConnection _server_con;

	Config &conf = Config::getConfig();
	conf.load(DEFAULT_CLIENT_CONFIG);

	conf.load(argv[1]);

	log(LOG_DEBUG, "Connecting ...");

	if(!_server_con.connect(conf["server"]["address"], conf["server"]["service"]))
		return -1;

	int status;
	if (argc == 2)
		status = process(_server_con, cin);
	else {
		ifstream in(argv[2]);
		if (!in) {
			cerr << "Cannot open " << argv[1] << "\n";
			status = -1;
		}
		else
			status = process(_server_con, in);
	}

	log(LOG_DEBUG, "Done.");

	_server_con.disconnect();
	return status;
}
