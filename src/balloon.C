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
#include <sstream>
#include <queue>
#include <string>
#include <cstring>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

using namespace std;

static pthread_mutex_t _runlock;
static pthread_cond_t _runcond;

static queue<string> notify;  // queue of unprinted notifications
static bool closed = false;   // set to true by the close notification
static string server;

static void balloon_notification(const MessageBlock* mb, void*) {
	time_t now = time(NULL);
	char time_buffer[64];
	struct tm time_tm;
        ostringstream msg;

	if (!server.empty() && (*mb)["server"] != server)
		return;  /* Wrong server, don't do anything */

	localtime_r(&now, &time_tm);
	strftime(time_buffer, sizeof(time_buffer), "%X", &time_tm);

	msg << time_buffer << " " << (*mb)["contestant"] << " solved "
	    << (*mb)["problem"] << " (server: " << (*mb)["server"] << ")";
	pthread_mutex_lock(&_runlock);
	notify.push(msg.str());
	pthread_cond_signal(&_runcond);
	pthread_mutex_unlock(&_runlock);
}

static void log_function(int priority, const char* format, va_list ap) {
	if(priority <= LOG_ERR) {
		vfprintf(stderr, format, ap);
		fprintf(stderr, "\n");
	}
}

static void server_close(const MessageBlock*, void*) {
	pthread_mutex_lock(&_runlock);
	closed = true;
	pthread_cond_signal(&_runcond);
	pthread_mutex_unlock(&_runlock);
}

int main(int argc, char **argv) {
	string username;
	string password;
	ServerConnection _server_con;

	register_log_listener(log_function);

	setup_sigsegv();
	Config &conf = Config::getConfig();
	conf.load(DEFAULT_CLIENT_CONFIG);
	char *home = getenv("HOME");
	if(home)
		conf.load(string(home) + "/.abacus");
	conf.load("abacus.conf");

	if(argc >= 3 && strcmp(argv[1], "-s") == 0)
	{
		server = argv[2];
		argc -= 2;
		argv += 2;
	}
	if(argc > 1)
		conf.load(argv[1]);

	cout << "Username: ";
	getline(cin, username);
	password = getpass("Password: ");

	pthread_mutex_init(&_runlock, NULL);
	pthread_cond_init(&_runcond, NULL);
	pthread_mutex_lock(&_runlock);

	if(!_server_con.connect(conf["server"]["address"], conf["server"]["service"]))
		return -1;

	if(!_server_con.auth(username, password))
		return -1;

	_server_con.registerEventCallback("close", server_close, NULL);
	_server_con.registerEventCallback("balloon", balloon_notification, NULL);

	if(!_server_con.watchBalloons(true)) {
		cout << "Error subscribing to the balloon notifications!\n";
		return -1;
	}

	cout << "All started up, waiting for balloon events ...\n";

	while (!closed || !notify.empty())
	{
		if (!notify.empty())
		{
			cout << notify.front() << "\n";
			cout << "Press enter to acknowledge" << flush;
			notify.pop();
			pthread_mutex_unlock(&_runlock);
			getline(cin, password);  // read any old string
			pthread_mutex_lock(&_runlock);
			cout << "Waiting for balloon events ...\n";
		}
		else
			pthread_cond_wait(&_runcond, &_runlock);
	}
	pthread_mutex_unlock(&_runlock);

	cout << "Server has disconnected - quitting.\n";

	_server_con.disconnect();
	pthread_mutex_destroy(&_runlock);
	pthread_cond_destroy(&_runcond);

	return 0;
}
