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
#include "dbcon.h"

#include <sstream>

using std::ostringstream;

SubmissionSupportModule::SubmissionSupportModule()
{
}

SubmissionSupportModule::~SubmissionSupportModule()
{
}

void SubmissionSupportModule::init()
{
}

bool SubmissionSupportModule::putSubmission(uint32_t sub_id, uint32_t user_id, uint32_t prob_id,
			uint32_t time, uint32_t server_id, char* content,
			uint32_t content_size, std::string language)
{
	DbCon *db = DbCon::getInstance();
	if(!db)
		return false;

	ostringstream query;
	query << "INSERT INTO Submission (submission_id, user_id, prob_id, time, server_id, content, language) VALUES(" << sub_id << ", " << user_id << ", " << prob_id << ", " << time << ", " << server_id << ", '" << db->escape_buffer((uint8_t*)content, content_size) << "', '" << db->escape_string(language) << "')";

	return db->executeQuery(query.str());
}

DEFINE_SUPPORT_MODULE_REGISTRAR(SubmissionSupportModule);
