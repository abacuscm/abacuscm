/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "viewclarificationrequestsub.h"
#include "ui_clarificationreply.h"

#include "logger.h"
#include "messageblock.h"
#include <qlabel.h>
#include <qtextbrowser.h>
#include <qtextedit.h>
#include <qcombobox.h>

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
		_server_con->clarificationReply(_clarification_request_id, pub, reply.answer->text());
		accept();
	}
}
