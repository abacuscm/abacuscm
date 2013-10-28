/**
 * Copyright (c) 2010, 2013 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */

#include "score.h"
#include <string>
#include <vector>
#include <cstdlib>

using namespace std;

Score::Score() : _id(0), _contestant(true), _total_solved(0), _total_time(0) {
}

void Score::setId(uint32_t id) {
	_id = id;
}

void Score::setUsername(const string &username) {
	_username = username;
}

void Score::setFriendlyname(const string &friendlyname) {
	_friendlyname = friendlyname;
}

void Score::setGroup(const string &group) {
	_group = group;
}

void Score::setContestant(bool contestant) {
	_contestant = contestant;
}

void Score::setTotalTime(time_t total_time) {
	_total_time = total_time;
}

void Score::setSolved(const vector<int> &solved) {
	_solved = solved;
	_total_solved = 0;
	for (size_t i = 0; i < solved.size(); i++)
		if (solved[i] > 0)
			_total_solved++;
}

void Score::setSolved(unsigned int problem, int attempts) {
	if (_solved.size() <= problem)
		_solved.resize(problem + 1);
	if (_solved[problem] > 0) _total_solved--;
	if (attempts > 0) _total_solved++;
	_solved[problem] = attempts;
}

int Score::getTotalAttempts() const {
	int total = 0;
	for (size_t i = 0; i < _solved.size(); i++)
		total += abs(_solved[i]);
	return total;
}
