/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "submissionsupportmodule.h"
#include "timersupportmodule.h"
#include "dbcon.h"
#include "messageblock.h"
#include "logger.h"

#include <sstream>
#include <string>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>

using std::ostringstream;

DEFINE_SUPPORT_MODULE(SubmissionSupportModule);

SubmissionSupportModule::SubmissionSupportModule()
{
}

SubmissionSupportModule::~SubmissionSupportModule()
{
}

void SubmissionSupportModule::init()
{
}

bool SubmissionSupportModule::submissionToMB(DbCon *db, AttributeList &s, MessageBlock &mb, const std::string &suffix) {
	uint32_t submission_id = strtoul(s["submission_id"].c_str(), NULL, 10);
	time_t time = strtoull(s["time"].c_str(), NULL, 10);
	uint32_t group_id = strtoul(s["group_id"].c_str(), NULL, 10);
	TimerSupportModule *timer = getTimerSupportModule();
	if (!timer) {
		log(LOG_CRIT, "Could not get timer support module");
		return false;
	}

	for (AttributeList::const_iterator a = s.begin(); a != s.end(); ++a)
		mb[a->first + suffix] = a->second;

	std::ostringstream time_str;
	time_str << timer->contestTime(group_id, time);
	mb["contesttime" + suffix] = time_str.str();

	RunResult resinfo;
	uint32_t type;
	std::string comment;
	std::ostringstream result_str;

	if(db->getSubmissionState(
			submission_id,
			resinfo,
			type,
			comment)) {
		result_str << (int)resinfo;
	} else {
		result_str << PENDING;
		comment = "Pending";
	}
	mb["result" + suffix] = result_str.str();
	mb["comment" + suffix] = comment;

	return true;
}

bool SubmissionSupportModule::putSubmission(uint32_t sub_id, uint32_t user_id, uint32_t prob_id,
			time_t time, uint32_t server_id, char* content,
			uint32_t content_size, std::string language)
{
	DbCon *db = DbCon::getInstance();
	if(!db)
		return false;

	ostringstream query;
	query << "INSERT INTO Submission (submission_id, user_id, prob_id, time, server_id, content, language) VALUES(" << sub_id << ", " << user_id << ", " << prob_id << ", " << time << ", " << server_id << ", '" << db->escape_buffer((uint8_t*)content, content_size) << "', '" << db->escape_string(language) << "')";

	bool res = db->executeQuery(query.str());
	db->release();

	return res;
}
