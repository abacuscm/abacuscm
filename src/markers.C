#include "markers.h"
#include "messageblock.h"
#include "clientconnection.h"
#include "logger.h"
#include "misc.h"
#include "dbcon.h"

#include <sstream>

Markers Markers::_instance;

Markers::Markers() {
	pthread_mutex_init(&_lock, NULL);
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
		if(_problems.empty()) {
			_markers.push_back(cc);
		} else {
			SubData *sd = _problems.front();
			_problems.pop_front();

			issue(cc, sd);
		}
	}
	pthread_mutex_unlock(&_lock);
}

void Markers::preemptMarker(ClientConnection* cc) {
	log(LOG_DEBUG, "Removing %p from available markers", cc);
	pthread_mutex_lock(&_lock);
	
	std::list<ClientConnection*>::iterator i1;
	std::map<ClientConnection*, SubData*>::iterator i2;

	if((i1 = list_find(_markers, cc)) != _markers.end())
		_markers.erase(i1);
	else if((i2 = _issued.find(cc)) != _issued.end()) {
		SubData *tmp = i2->second;
		_issued.erase(i2);
		enqueueSubmission(tmp);
	}
	pthread_mutex_unlock(&_lock);
}

void Markers::enqueueSubmission(SubData *sd) {
	// we already have the lock at this point in time.
	if(_markers.empty()) {
		_problems.push_back(sd);
	} else {
		ClientConnection *cc = _markers.front();
		_markers.pop_front();

		issue(cc, sd);
	}
}

void Markers::enqueueSubmission(uint32_t user_id, uint32_t prob_id, uint32_t time) {
	log(LOG_DEBUG, "Enqueueing submission (%d,%d,%d) into Marking system", user_id, prob_id, time);

	SubData *sd = new SubData;
	sd->user_id = user_id;
	sd->prob_id = prob_id;
	sd->time = time;
	sd->issued = NULL;

	// this is more a 'token' resistance than anything else and is only
	// meant to prevent accidental errors.
	sd->hash = rand();

	pthread_mutex_lock(&_lock);
	enqueueSubmission(sd);
	pthread_mutex_unlock(&_lock);
}

void Markers::issue(ClientConnection* cc, SubData* sd) {
	// We already have the lock.
	char* content;
	int length;
	std::string language;

	DbCon *db = DbCon::getInstance();
	if(!db) {
		log(LOG_CRIT, "Markers::issue() was unable to obtain a conenction to the database - this is _VERY_ serious!");
		_problems.push_back(sd);
		_markers.push_back(cc);
		return;
	}

	bool has_data = db->retrieveSubmission(sd->user_id, sd->prob_id, sd->time, &content, &length, language);
	db->release();

	if(!has_data) {
		log(LOG_CRIT, "Markers::issue() was unable to obtain the actual submission data - this is _VERY_ serious!");
		_problems.push_back(sd);
		_markers.push_back(cc);
		return;
	}

	log(LOG_DEBUG, "Issueing (%d,%d,%d) to %p", sd->user_id, sd->prob_id, sd->time, cc);

	std::ostringstream tmp;
	
	MessageBlock mb("mark");
	tmp << sd->hash;
	mb["submission_hash"] = tmp.str();
	tmp.str(""); tmp << sd->prob_id;
	mb["prob_id"] = tmp.str();
	mb["language"] = language;
	mb.setContent(content, length);

	if(cc->sendMessageBlock(&mb)) {
		sd->issued = cc;
		_issued[cc] = sd;
	} else 
		enqueueSubmission(sd);
}

bool Markers::putMark(ClientConnection* cc, MessageBlock*) {
	NOT_IMPLEMENTED();
	return cc->sendError("Not implemented");
}
