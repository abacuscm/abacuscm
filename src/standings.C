/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id: balloon.C 368 2006-10-09 21:18:36Z bmerry $
 */
#include "logger.h"
#include "config.h"
#include "sigsegv.h"
#include "serverconnection.h"
#include "messageblock.h"

#include <iostream>
#include <fstream>
#include <queue>
#include <string>
#include <algorithm>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

using namespace std;

static pthread_mutex_t _runlock;
static pthread_cond_t _runcond;
static bool closed = false;   // set to true by the close notification

static void log_function(int priority, const char* format, va_list ap) {
	if(priority <= LOG_ERR) {
		vfprintf(stderr, format, ap);
		fprintf(stderr, "\n");
	}
}

static void standings_update(const MessageBlock *, void*) {
	pthread_mutex_lock(&_runlock);
	pthread_cond_signal(&_runcond);
	pthread_mutex_unlock(&_runlock);
}

static void server_close(const MessageBlock*, void*) {
	pthread_mutex_lock(&_runlock);
	closed = true;
	pthread_cond_signal(&_runcond);
	pthread_mutex_unlock(&_runlock);
}

static void update_standings(ServerConnection &_server_con, const string &fname) {
	Grid data = _server_con.getStandings();
	string tmpname = fname + ".tmp";

	ofstream out(tmpname.c_str());
	if (!out)
	{
		cerr << "Warning: failed to open " << tmpname << "!\n";
		return;
	}
	for (Grid::iterator r = data.begin(); r != data.end(); r++)
	{
		for (list<string>::iterator c = r->begin(); c != r->end(); c++)
		{
			// There shouldn't be any newlines or tabs, but turn them into
			// spaces just to be safe.
			replace(c->begin(), c->end(), '\t', ' ');
			replace(c->begin(), c->end(), '\n', ' ');
			if (c != r->begin()) out << "\t";
			out << *c;
		}
		out << "\n";
	}
	if (!out)
	{
		cerr << "Warning: error writing " << tmpname << "!\n";
		out.close();
		unlink(tmpname.c_str());
	}
	else
	{
		out.close();
		if (rename(tmpname.c_str(), fname.c_str()) != 0)
		{
			cerr << "Error: could not rename " << tmpname << " to " << fname << "!\n";
			unlink(tmpname.c_str());
		}
		else
			cout << "Successfully wrote standings to " << fname << "\n";
	}
}

int main(int argc, char **argv) {
	string username;
	string password;
	const char *fname = "standings.txt";

	register_log_listener(log_function);

	setup_sigsegv();
	Config &conf = Config::getConfig();
	conf.load(DEFAULT_CLIENT_CONFIG);
	char *home = getenv("HOME");
	if(home)
		conf.load(string(home) + "/.abacus");
	conf.load("abacus.conf");

	if(argc > 1)
		conf.load(argv[1]);
	if (argc > 2)
		fname = argv[2];

	cout << "Username: ";
	getline(cin, username);
	password = getpass("Password: ");

	pthread_mutex_init(&_runlock, NULL);
	pthread_cond_init(&_runcond, NULL);
	pthread_mutex_lock(&_runlock);

	for (;;)
	{
		ServerConnection _server_con;
		if(!_server_con.connect(conf["server"]["address"], conf["server"]["service"]))
		{
			cout << "Could not connect to server; sleeping for 10 seconds\n";
			sleep(10);
			continue;
		}

		if(!_server_con.auth(username, password))
		{
			cout << "Authentication failed! Sleeping for 10 seconds\n";
			_server_con.disconnect();
			sleep(10);
			return -1;
		}

		_server_con.registerEventCallback("close", server_close, NULL);
		_server_con.registerEventCallback("standings", standings_update, NULL);

		update_standings(_server_con, fname);
		for (;;)
		{
			pthread_cond_wait(&_runcond, &_runlock);
			if (closed) break;
			update_standings(_server_con, fname);
		}
		pthread_mutex_unlock(&_runlock);

		cout << "Server has disconnected. Sleeping for 10 seconds.\n";

		_server_con.disconnect();
		sleep(10);
	}
	pthread_mutex_destroy(&_runlock);
	pthread_cond_destroy(&_runcond);

	return 0;
}
