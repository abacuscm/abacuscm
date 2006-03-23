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

static pthread_mutex_t _runlock;
static pthread_cond_t _runcond;

static void balloon_notification(const MessageBlock* mb, void*) {
	time_t now = time(NULL);
	char time_buffer[64];
	struct tm time_tm;

	localtime_r(&now, &time_tm);
	strftime(time_buffer, sizeof(time_buffer), "%X", &time_tm);

	printf("%s: %s solved %s (server: %s)\n", time_buffer, (*mb)["contestant"].c_str(),
			(*mb)["problem"].c_str(), (*mb)["server"].c_str());
}

static void log_function(int priority, const char* format, va_list ap) {
	if(priority <= LOG_ERR) {
		vfprintf(stderr, format, ap);
		fprintf(stderr, "\n");
	}
}

static void server_close(const MessageBlock*, void*) {
	pthread_mutex_lock(&_runlock);
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
	if(argc > 1)
		conf.load(argv[1]);

	cout << "Username: ";
	getline(cin, username);
	password = getpass("Password: ");

	pthread_mutex_init(&_runlock, NULL);
	pthread_cond_init(&_runcond, NULL);

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

	pthread_mutex_lock(&_runlock);
	pthread_cond_wait(&_runcond, &_runlock);
	pthread_mutex_unlock(&_runlock);

	cout << "Server has disconnected - quitting.\n";

	pthread_mutex_destroy(&_runlock);
	pthread_cond_destroy(&_runcond);

	_server_con.disconnect();
	return 0;
}
