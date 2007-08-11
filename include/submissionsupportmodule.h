/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __SUBMISSIONSUPPORTMODULE_H__
#define __SUBMISSIONSUPPORTMODULE_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "supportmodule.h"

#include <stdint.h>

class SubmissionSupportModule : public SupportModule {
	DECLARE_SUPPORT_MODULE(SubmissionSupportModule);
private:
	SubmissionSupportModule();
	virtual ~SubmissionSupportModule();

public:
/*	class Submission {
	private:
	public:
		uint32_t submission_id();
		uint32_t server_id();
		uint32_t user_id();
	};*/

	virtual void init();

	bool putSubmission(uint32_t sub_id, uint32_t user_id, uint32_t prob_id,
			uint32_t time, uint32_t server_id, char* content,
			uint32_t content_size, std::string language);

};

DEFINE_SUPPORT_MODULE_GETTER(SubmissionSupportModule);

#endif
