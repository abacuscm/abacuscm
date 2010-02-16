/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "acmconfig.h"
#include "markers.h"
#include "messageblock.h"
#include "clientconnection.h"
#include "logger.h"
#include "misc.h"
#include "dbcon.h"

#include <sstream>

Markers Markers::_instance;

void *checkForTimeoutsThread(void *) {
	while (!Markers::getInstance().shouldTerminate()) {
		Markers::getInstance().checkForTimeouts();

		// Sleep for 1 second and then have another look
		sleep(1);
	}

	return NULL;
}

Markers::Markers() {
	pthread_mutex_init(&_lock, NULL);
	_shouldTerminate = 0;
}

Markers::~Markers() {
	pthread_mutex_destroy(&_lock);
}

Markers& Markers::getInstance() {
	return _instance;
}

void Markers::enqueueMarker(ClientConnection* cc) {
	log(LOG_DEBUG, "Adding marker %p", cc);
	pthread_mutex_lock(&_lock);
	if(_issued.find(cc) != _issued.end()) {
		log(LOG_NOTICE, "Marker %p currently has issued submissions", cc);
	} else if(list_find(_markers, cc) != _markers.end()) {
		log(LOG_NOTICE, "Marker %p is already enqueued!", cc);
	} else {
		real_enqueueMarker(cc);
	}
	pthread_mutex_unlock(&_lock);
}

void Markers::preemptMarker(ClientConnection* cc) {
	pthread_mutex_lock(&_lock);

	std::list<ClientConnection*>::iterator i1;
	std::map<ClientConnection*, uint32_t>::iterator i2;

	if((i1 = list_find(_markers, cc)) != _markers.end()) {
		log(LOG_DEBUG, "Removing %p from available markers - was not issued anything", cc);
		_markers.erase(i1);
	}
	else if((i2 = _issued.find(cc)) != _issued.end()) {
		uint32_t tmp = i2->second;
		log(LOG_DEBUG, "Removing %p from available markers - was issued %u", cc, tmp);
		_issued.erase(i2);
		real_enqueueSubmission(tmp);
	}
	pthread_mutex_unlock(&_lock);
}

void Markers::real_enqueueSubmission(uint32_t sd) {
	log(LOG_DEBUG, "Enqueueing submission %u for marking.", sd);
	// we already have the lock at this point in time.
	if(_markers.empty()) {
		_problems.push_back(sd);
	} else {
		ClientConnection *cc = _markers.front();
		_markers.pop_front();

		issue(cc, sd);
	}
}

void Markers::real_enqueueMarker(ClientConnection* cc) {
	log(LOG_DEBUG, "Enqueuing marker %p", cc);
	if(_problems.empty()) {
		_markers.push_back(cc);
	} else {
		uint32_t sd = _problems.front();
		_problems.pop_front();

		issue(cc, sd);
	}
}

void Markers::enqueueSubmission(uint32_t submission_id) {
	pthread_mutex_lock(&_lock);
	real_enqueueSubmission(submission_id);
	pthread_mutex_unlock(&_lock);
}

void Markers::issue(ClientConnection* cc, uint32_t sd) {
	// We already have the lock.
	DbCon *db = DbCon::getInstance();
	if(!db) {
		log(LOG_CRIT, "Markers::issue() was unable to obtain a conenction to the database - this is _VERY_ serious!");
		_problems.push_back(sd);
		_markers.push_back(cc);
		return;
	}

	char* content;
	int length;
	std::string language;
	uint32_t prob_id;

	bool has_data = db->retrieveSubmission(sd, &content, &length, language, &prob_id);
	db->release();db=NULL;

	if(!has_data) {
		log(LOG_CRIT, "Markers::issue() was unable to obtain the actual submission data (submission_id=%u) - this is _VERY_ serious!", sd);
		_problems.push_back(sd);
		_markers.push_back(cc);
		return;
	}

	log(LOG_DEBUG, "Issuing submission %u to %p", sd, cc);

	std::ostringstream tmp;

	MessageBlock mb("mark");
	tmp << sd;
	mb["submission_id"] = tmp.str();
	tmp.str(""); tmp  << prob_id;
	mb["prob_id"] = tmp.str();
	mb["language"] = language;
	mb.setContent(content, length);
	delete []content;

	log(LOG_DEBUG, "About to issue %u to %p", sd, cc);
	if(cc->sendMessageBlock(&mb)) {
		_issued[cc] = sd;
		_issue_times.push(std::make_pair(time(NULL), std::make_pair(cc, sd)));
		log(LOG_DEBUG, "Issued submission %u to %p", sd, cc);
	} else {
		enqueueSubmission(sd);
		log(LOG_DEBUG, "Submission of %u to %p failed; returning submission to queue", sd, cc);
	}
}

uint32_t Markers::hasIssued(ClientConnection*cc) {
	uint32_t result = 0;

	pthread_mutex_lock(&_lock);
	std::map<ClientConnection*, uint32_t>::const_iterator i = _issued.find(cc);
	if(i != _issued.end())
		result = i->second;
	pthread_mutex_unlock(&_lock);

	return result;
}

void Markers::notifyMarked(ClientConnection* cc, uint32_t submission_id) {
	pthread_mutex_lock(&_lock);

	std::map<ClientConnection*, uint32_t>::iterator i = _issued.find(cc);
	if(i != _issued.end()) {
		if(submission_id != i->second) {
			log(LOG_WARNING, "Marker %p marked submission %u instead of %u, re-enqueueing %u", cc, submission_id, i->second, i->second);
			real_enqueueSubmission(i->second);
		}

		_issued.erase(i);

		// chuck the submission_id - it has now been marked.
		// re-enqueue the ClientConnection.
		log(LOG_DEBUG, "Adding marker %p back to queue after successful mark of submission %u", cc, submission_id);
		real_enqueueMarker(cc);
	}
	pthread_mutex_unlock(&_lock);
}

void Markers::checkForTimeouts() {
	pthread_mutex_lock(&_lock);

	while (!_issue_times.empty() && ((time(NULL) - _issue_times.top().first) > (time_t) _timeout)) {
		// The earliest submitted marking job has timed out
		ClientConnection *cc = _issue_times.top().second.first;
		uint32_t submission_id = _issue_times.top().second.second;
		_issue_times.pop();

		std::map<ClientConnection*, uint32_t>::iterator i = _issued.find(cc);
		if (i == _issued.end() || i->second != submission_id)
			continue;

		log(LOG_WARNING, "Marker %p timed out in marking submission %u, re-enqueuing submission and dropping marker client connection from pool", cc, submission_id);

		_issued.erase(i);
		real_enqueueSubmission(submission_id);
	}

	pthread_mutex_unlock(&_lock);
}

void Markers::startTimeoutCheckingThread() {
	_timeout = atoll(Config::getConfig()["markers"]["timeout"].c_str());
	if (_timeout == 0) {
		// Disable timeout checking if the timeout is 0
		return;
	}

	_shouldTerminate = 0;
	pthread_create(&_timeout_checker, NULL, checkForTimeoutsThread, NULL);
}

bool Markers::shouldTerminate() {
	return (_shouldTerminate != 0);
}

void Markers::shutdown() {
	_shouldTerminate = 1;

	// Only join the timeout thread if we created it
	if (_timeout > 0) {
		pthread_join(_timeout_checker, NULL);
	}
}


