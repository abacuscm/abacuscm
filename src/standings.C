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
#include "acmconfig.h"
#include "serverconnection.h"
#include "messageblock.h"
#include "misc.h"

#include <iostream>
#include <fstream>
#include <queue>
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

using namespace std;

class Score {
private:
	uint32_t _id;
	bool _contestant;
	uint32_t _total_time;
	int _total_solved;

	vector<string> _raw;

public:
	bool operator<(const Score &other) const {
		if (_total_solved != other._total_solved)
			return _total_solved > other._total_solved;
		else
			return _total_time < other._total_time;
	}

	const vector<string> &getRaw() const { return _raw; }
	uint32_t getId() const { return _id; }
	bool isContestant() const { return _contestant; }

	Score() {}
	Score(const list<string> &row);
};

Score::Score(const list<string> &row) : _raw(row.begin(), row.end()) {
	_id = strtoul(_raw[STANDING_RAW_ID].c_str(), NULL, 10);
	_contestant = strtoul(_raw[STANDING_RAW_CONTESTANT].c_str(), NULL, 10) != 0;
	_total_time = strtoull(_raw[STANDING_RAW_TOTAL_TIME].c_str(), NULL, 10);
	_total_solved = strtol(_raw[STANDING_RAW_TOTAL_SOLVED].c_str(), NULL, 10);
}

typedef map<uint32_t, Score> ScoreMap;

struct CompareScore {
	bool operator()(ScoreMap::const_iterator a, ScoreMap::const_iterator b) const {
		return a->second < b->second;
	}
};

static pthread_mutex_t _runlock;
static pthread_cond_t _runcond;
static bool closed = false;   // set to true by the close notification

static vector<ScoreMap::const_iterator> ordered;
static map<uint32_t, Score> scores;
static vector<string> header;

static void log_function(int priority, const char* format, va_list ap) {
	if(priority <= LOG_ERR) {
		vfprintf(stderr, format, ap);
		fprintf(stderr, "\n");
	}
}

static void grid_to_standings(const Grid &grid) {
	if (grid.empty())
		return;

	Grid::const_iterator row = grid.begin();
	header = vector<string>(row->begin(), row->end());

	for (++row; row != grid.end(); ++row) {
		Score s(*row);
		if (!s.isContestant())
			continue;
		pair<ScoreMap::iterator, bool> add = scores.insert(make_pair(s.getId(), s));
		if (add.second)
			ordered.push_back(add.first);
		else
			add.first->second = s;
	}
	sort(ordered.begin(), ordered.end(), CompareScore());
}

static void standings_update(const MessageBlock *mb, void*) {
	pthread_mutex_lock(&_runlock);
	if (mb) {
		Grid grid = ServerConnection::gridFromMB(*mb);
		grid_to_standings(grid);
	}
	pthread_cond_signal(&_runcond);
	pthread_mutex_unlock(&_runlock);
}

static void server_close(const MessageBlock*, void*) {
	pthread_mutex_lock(&_runlock);
	closed = true;
	pthread_cond_signal(&_runcond);
	pthread_mutex_unlock(&_runlock);
}

static string safe_string(string s) {
	// There shouldn't be any newlines or tabs, but turn them into
	// spaces just to be safe.
	replace(s.begin(), s.end(), '\t', ' ');
	replace(s.begin(), s.end(), '\n', ' ');
	return s;
}

static void update_standings(const string &fname) {
	string tmpname = fname + ".tmp";

	ofstream out(tmpname.c_str());
	if (!out)
	{
		cerr << "Warning: failed to open " << tmpname << "!\n";
		return;
	}
	for (vector<string>::const_iterator i = header.begin(); i != header.end(); ++i) {
		if (i != header.begin())
			out << "\t";
		out << safe_string(*i);
	}
	out << "\n";
	for (vector<ScoreMap::const_iterator>::const_iterator i = ordered.begin(); i != ordered.end(); ++i)
	{
		const vector<string> &raw = (*i)->second.getRaw();
		for (vector<string>::const_iterator c = raw.begin(); c != raw.end(); c++)
		{
			if (c != raw.begin()) out << "\t";
			out << safe_string(*c);
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
		header.clear();
		ordered.clear();
		scores.clear();

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
		_server_con.registerEventCallback("updatestandings", standings_update, NULL);

		grid_to_standings(_server_con.getStandings());
		update_standings(fname);
		for (;;)
		{
			pthread_cond_wait(&_runcond, &_runlock);
			if (closed) break;
			update_standings(fname);
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
