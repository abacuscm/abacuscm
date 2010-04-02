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

public:
	JudgeDecisionDialog() : _result(JUDGE) {}
	virtual ~JudgeDecisionDialog() {}

	RunResult getResult() { return _result; }
};

#endif
