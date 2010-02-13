/**
 * Copyright (c) 2005 - 2006, 2010 Kroon Infomation Systems,
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
#include <set>
#include <string>
#include <utility>
#include <qiconset.h>

class QCheckBox;
class QListViewItem;
class QTimer;
class QFileDialog;
class Submit;
class ClarificationRequestItem;
class ClarificationItem;
class SubmissionItem;
class StandingItem;

typedef struct {
	int prio_level;
	QString msg;
} LogMessage;

class MainWindow : public MainWindowBase {
private:
	QIconSet quietIcon, alertIcon;

	LoginDialog _login_dialog;
	Submit *_submit_dialog;
	QFileDialog *_submit_file_dialog;

	ServerConnection _server_con;

	std::string _active_type;

	std::set<uint32_t> _subscribed_problems;

	/* Clock-related stuff */
	QTimer *timer;
	time_t projected_stop;

	/* Look up internal data by ID */
	std::map<uint32_t, ClarificationRequestItem *> clarificationRequestMap;
	std::map<uint32_t, ClarificationItem *> clarificationMap;
	std::map<uint32_t, SubmissionItem *> submissionMap;
	std::map<uint32_t, StandingItem *> standingMap;

	void triggerType(std::string type, bool status);
	void switchType(std::string type);

	/* Indicate that a tab does/does not have new information */
	void setAlert(QWidget *tab);
	void setQuiet(QWidget *tab);

	/* Find states requested for judge's submission filter */
	std::set<int> getWantedStates() const;
	/* Helpers to set information from either a bulk reply or an incremental
	 * notification.
	 */
	template<typename T> void setClarificationRequest(ClarificationRequestItem *item, T &cr);
	template<typename T> void setClarification(ClarificationItem *item, T &cr);
	template<typename T> void setSubmission(SubmissionItem *item, bool filter,
											const std::set<int> &wanted_states,
											T &submission);
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
	void updateStandings(const MessageBlock *mb);
	void updateSubmissions(const MessageBlock *mb);
	void updateClarificationRequests(const MessageBlock *mb = NULL);
	void updateClarifications(const MessageBlock *mb = NULL);
	void updateStatus(const MessageBlock *mb = NULL);
	void serverDisconnect(const MessageBlock *mb = NULL);

	// Overloads of functions in MainWindowBase
	virtual void sortStandings();
	virtual void refilterSubmissions();
	virtual void updateStandings() { updateStandings(NULL); }
	virtual void updateSubmissions() { updateSubmissions(NULL); }

	std::string getActiveType();
};

#endif
