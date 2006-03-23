/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "logger.h"
#include "config.h"
#include "sigsegv.h"
#include "serverconnection.h"
#include "messageblock.h"

#include <iostream>
#include <string>
#include <pthread.h>
#include <time.h>

using namespace std;

int main(int argc, char **argv) {
	if(argc < 7) {
		cerr << "USAGE: createusers config username password newaccount newpassword type\n";
		return -1;
	}

	string username = argv[2];
	string password = argv[3];
	string newusername = argv[4];
	string newpassword = argv[5];
	string type = argv[6];

	ServerConnection _server_con;

	setup_sigsegv();
	Config &conf = Config::getConfig();
	conf.load(DEFAULT_CLIENT_CONFIG);

	conf.load(argv[1]);

	log(LOG_DEBUG, "Connecting ...");

	if(!_server_con.connect(conf["server"]["address"], conf["server"]["service"]))
		return -1;

	log(LOG_DEBUG, "Authenticating ...");

	if(!_server_con.auth(username, password))
		return -1;

	log(LOG_DEBUG, "Creating user ...");

	if(!_server_con.createuser(newusername, newpassword, type))
		return -1;

	log(LOG_DEBUG, "Done.");

	_server_con.disconnect();
	return 0;
}
