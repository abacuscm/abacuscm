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

static bool match(const MessageBlock *ret, const MessageBlock *expect) {
	if (ret->action() != expect->action()) {
		return false;
	}

	for (MessageBlock::const_iterator i = ret->begin(); i != ret->end(); ++i) {
		string e;
		if (expect->hasAttribute(i->first)) {
			e = (*expect)[i->first];
		} else if (expect->hasAttribute("*")) {
			e = (*expect)["*"];
		} else {
			return false;
		}

		if (e != "*" && e != i->second) {
			return false;
		}
	}

	for (MessageBlock::const_iterator i = expect->begin(); i != expect->end(); ++i) {
		if (i->first != "*" && !ret->hasAttribute(i->first)) {
			return false;
		}
	}

	return true;
}

void batch_sendMB(ServerConnection &_server_con, MessageBlock *&mb, MessageBlock *&expect,
				  vector<string> &content, vector<string> &expect_content) {
	string all_content;

	if (mb == NULL)
		return;

	if (!content.empty()) {
		all_content = accumulate(content.begin(), content.end(), string());
		mb->setContent(all_content.c_str(), all_content.size(), false);
	}
	if (expect != NULL && !expect_content.empty()) {
		all_content = accumulate(expect_content.begin(), expect_content.end(), string());
		expect->setContent(all_content.c_str(), all_content.size(), false);
	}

	log(LOG_DEBUG, "Sending %s", mb->action().c_str());
	MessageBlock *ret = _server_con.sendMB(mb);
	if (ret == NULL) {
		log(LOG_WARNING, "Did not receive a response to");
		mb->dump();
		exit(1);
	} else {
		log(LOG_DEBUG, "Received reply");
		ret->dump();
		if (expect != NULL) {
			if (!match(ret, expect)) {
				log(LOG_WARNING, "Did not match expected reply");
				expect->dump();
				exit(1);
			}
		}
	}

	delete ret;
	delete mb;
	delete expect;
	mb = NULL;
	expect = NULL;
	content.clear();
	expect_content.clear();
}

/* Appends a line to a record. If the line indicates the start of a new
 * record, returns the old value. Otherwise returns NULL.
 */
static MessageBlock *append(const std::string line, MessageBlock *&cur, vector<string> &content) {
	MessageBlock *ret = NULL;

	string::size_type split = line.find(':');
	if (split == string::npos) {
		split = line.find('<');
		if (split == string::npos) {
			/* Start of a new record */
			ret = cur;
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
				exit(1);
			}
			else {
				buffer << inf.rdbuf();
				if (!inf) {
					log(LOG_ERR, "Cannot read %s", fname.c_str());
					exit(1);
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
			exit(1);
		}
		(*cur)[line.substr(0, split)] = line.substr(split + 1);
	}
	return ret;
}

static int process(ServerConnection &_server_con, istream &in) {
	string line;
	MessageBlock *cur = NULL;
	MessageBlock *expect = NULL;
	vector<string> cur_content;
	vector<string> expect_content;
	int lineno = 0;

	while (getline(in, line)) {
		lineno++;
		if (line == "") {
			/* Useful in interactive mode for flushing a record */
			batch_sendMB(_server_con, cur, expect, cur_content, expect_content);
			continue;
		}
		else if (line[0] == '#') {
			/* Comment */
			continue;
		}
		else if (line[0] == '?') {
			/* Assertion of return value */
			if (cur == NULL) {
				log(LOG_ERR, "Assertion without message (line %d)", lineno);
				return 1;
			}
			line.erase(0, 1);
			MessageBlock *old = append(line, expect, expect_content);
			if (old != NULL) {
				log(LOG_ERR, "Assertion without message (line %d)", lineno);
				return 1;
			}
		}
		else
		{
			MessageBlock *old = append(line, cur, cur_content);
			if (old != NULL) {
				batch_sendMB(_server_con, old, expect, cur_content, expect_content);
			}
		}
	}
	batch_sendMB(_server_con, cur, expect, cur_content, expect_content);
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
