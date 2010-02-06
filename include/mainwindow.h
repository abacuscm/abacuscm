/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __MAINWINDOW_H__
#define __MAINWINDOW_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "ui_mainwindowbase.h"
#include "ui_logindialog.h"

#include "serverconnection.h"

#include <map>
#include <string>
#include <utility>

class QCheckBox;
class QListViewItem;
class QTimer;
class QFileDialog;
class Submit;

typedef struct {
	int prio_level;
	QString msg;
} LogMessage;

class MainWindow : public MainWindowBase {
private:
	LoginDialog _login_dialog;
	Submit *_submit_dialog;
	QFileDialog *_submit_file_dialog;

	ServerConnection _server_con;

	std::string _active_type;

	/* Clock-related stuff */
	QTimer *timer;
	time_t projected_stop;

	/* Clarification stuff. The list box holds just the summary, so
	 * we need to map the ID to the full text in each case.
	 */
	std::map<std::string, std::string> fullClarificationRequests;
	std::map<std::string, std::pair<std::string, std::string> > fullClarifications;

	void triggerType(std::string type, bool status);
	void switchType(std::string type);

	template<typename T> void setClarification(QListViewItem *item, T &cr);
protected:
	virtual void doHelpAbout();
	virtual void doFileConnect();
	virtual void doFileDisconnect();
	virtual void doAdminCreateUser();
	virtual void doAdminProblemConfig();
	virtual void doAdminStartStop();
	virtual void doSubmit();
	virtual void doClarificationRequest();
	virtual void doShowClarificationRequest(QListViewItem*);
	virtual void doShowClarificationReply(QListViewItem*);
	virtual void doTimer();
	virtual void tabChanged(QWidget*);
	virtual void doChangePassword();
	virtual void doJudgeSubscribeToProblems();
	virtual void submissionHandler(QListViewItem *);
	virtual void toggleBalloonPopups(bool);

	virtual void customEvent(QCustomEvent *ev);
public:
	MainWindow();
	~MainWindow();

	// made public so that my functors can get to them since
	// we can't cast member function pointers to void* - no idea
	// why though - can probably memcpy them but that would be
	// even uglier.
	void updateStandings(const MessageBlock *mb = NULL);
	virtual void updateSubmissions(const MessageBlock *mb = NULL);
	void updateClarificationRequests(const MessageBlock *mb = NULL);
	void updateClarifications(const MessageBlock *mb = NULL);
	void updateStatus(const MessageBlock *mb = NULL);
	void serverDisconnect(const MessageBlock *mb = NULL);

	std::string getActiveType();
};

#endif
