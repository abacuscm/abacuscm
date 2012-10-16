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
#include "acmconfig.h"
#include "serverconnection.h"
#include "messageblock.h"

#include <iostream>
#include <string>
#include <pthread.h>
#include <time.h>

using namespace std;

int main(int argc, char **argv) {
	if(argc != 8) {
		cerr << "USAGE: " << *argv << " config username password newaccount newname newpassword type\n";
		return -1;
	}

	string username = argv[2];
	string password = argv[3];
	string newusername = argv[4];
	string newfriendlyname = argv[5];
	string newpassword = argv[6];
	string type = argv[7];

	ServerConnection::init();
	ServerConnection _server_con;

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

	if(!_server_con.createuser(newusername, newfriendlyname, newpassword, type))
		return -1;

	log(LOG_DEBUG, "Done.");

	_server_con.disconnect();
	return 0;
}
