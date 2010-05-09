/**
 * Copyright (c) 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */

#ifndef __JUDGEDECISIONDIALOG_H__
#define __JUDGEDECISIONDIALOG_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "ui_judgedecisiondialogbase.h"
#include "misc.h"
#include <qstring.h>
#include <string>
#include <qtextbrowser.h>

class JudgeDecisionDialog : public JudgeDecisionDialogBase
{
private:
	RunResult _result;

	void setResult(RunResult result) { _result = result; }

protected:
	virtual void deferred() { reject(); }
	virtual void correct() { setResult(CORRECT); accept(); }
	virtual void wrong() { setResult(WRONG); accept(); }
	virtual void formatError() { setResult(FORMAT_ERROR); accept(); }

	virtual void FileSelector_activated( const QString &key ) {
		if (key == "Select file to view...")
			FileData->setText("");
		else {
			const std::string &text = data[key];
			std::string display = "";
			for (std::string::size_type i = 0; i < text.length(); i++) {
				if (::isprint(text[i]) || ::isspace(text[i]))
					display += text[i];
				else {
					char buffer[10];
					sprintf(buffer, "\\%03o", (unsigned char) text[i]);
					log(LOG_DEBUG, "buffer: %s", buffer);
					display += buffer;
				}
			}
			FileData->setText(display);
		}
	}

public:
	JudgeDecisionDialog() : _result(JUDGE) {}
	virtual ~JudgeDecisionDialog() {}

	RunResult getResult() { return _result; }
};

#endif
