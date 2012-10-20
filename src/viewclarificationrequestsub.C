/**
 * Copyright (c) 2005 - 2006, 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "viewclarificationrequestsub.h"
#include "clarificationreply.h"

#include "logger.h"
#include "messageblock.h"
#include <Qt/qlabel.h>
#include <Qt/q3textbrowser.h>
#include <Qt/q3textedit.h>
#include <Qt/qcombobox.h>

ViewClarificationRequestSub::ViewClarificationRequestSub(uint32_t id, ServerConnection *server_con) {
	_clarification_request_id = id;
	_server_con = server_con;
}

ViewClarificationRequestSub::~ViewClarificationRequestSub() {
}

void ViewClarificationRequestSub::doReply() {
	ClarificationReply reply;

	reply.problem->setText(problem->text());
	reply.question->setText(question->text());
	reply.answer->setFocus();

	if (reply.exec())
	{
		bool pub = reply.visibility->currentItem() == 1;
		_server_con->clarificationReply(_clarification_request_id, pub, (const char *) reply.answer->text().utf8());
		accept();
	}
}
