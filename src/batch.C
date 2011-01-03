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
#include <numeric>
#include <map>
#include <stdexcept>
#include <cassert>

using namespace std;

void batch_sendMB(ServerConnection &_server_con, MessageBlock *&mb,
				  vector<string> &content) {
	string all_content;

	if (mb == NULL)
		return;

	if (!content.empty()) {
		all_content = accumulate(content.begin(), content.end(), string());
		mb->setContent(all_content.c_str(), all_content.size(), false);
	}

	log(LOG_DEBUG, "Sending %s", mb->action().c_str());
	MessageBlock *ret = _server_con.sendMB(mb);
	if (ret == NULL) {
		log(LOG_WARNING, "Did not receive a response to");
		mb->dump();
		return;
	}
	else
	{
		log(LOG_DEBUG, "Received reply");
		ret->dump();
	}

	delete ret;
	delete mb;
	mb = NULL;
	content.clear();
}

static int process(ServerConnection &_server_con, istream &in) {
	string line;
	MessageBlock *cur = NULL;
	vector<string> content;
	int lineno = 0;

	while (getline(in, line)) {
		lineno++;
		if (line == "") {
			/* Useful in interactive mode for flushing a record */
			batch_sendMB(_server_con, cur, content);
			continue;
		}
		else if (line[0] == '#') {
			/* Comment */
			continue;
		}

		string::size_type split = line.find(':');
		if (split == string::npos) {
			split = line.find('<');
			if (split == string::npos) {
				/* Start of a new record */
				batch_sendMB(_server_con, cur, content);
				cur = new MessageBlock(line);
			}
			else {
				/* File input */
				string field = line.substr(0, split);
				string fname = line.substr(split + 1);
				ostringstream buffer;
				ifstream inf(fname.c_str());
				if (!inf) {
					log(LOG_ERR, "Cannot open %s", fname.c_str());
				}
				else {
					buffer << inf.rdbuf();
					if (!buffer) {
						log(LOG_ERR, "Cannot read %s", fname.c_str());
					}
					else {
						inf.close();

						/* Extract the base name */
						string::size_type slash = fname.rfind('/');
						if (slash != string::npos)
							fname = fname.substr(slash + 1);
						if (field != "") {
							/* Encode the location, for the set problem attribs message */
							ostringstream value;
							size_t offset = 0;
							for (size_t i = 0; i < content.size(); i++)
								offset += content[i].size();
							value << offset << " " << buffer.str().size() << " " << fname;
							(*cur)[field] = value.str();
						}
						content.push_back(buffer.str());
					}
				}
			}
		}
		else {
			/* Header */
			if (cur == NULL) {
				log(LOG_ERR, "Message data with no message header");
				return -1;
			}
			(*cur)[line.substr(0, split)] = line.substr(split + 1);
		}
	}
	batch_sendMB(_server_con, cur, content);
	return 0;
}

int main(int argc, const char **argv) {
	if (argc < 2 || argc > 3) {
		cerr << "Usage: " << argv[0] << " config [messagefile]\n";
		return -1;
	}

	ServerConnection::init();
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
