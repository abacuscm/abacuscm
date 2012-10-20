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
#include "logindialog.h"

#include "serverconnection.h"
#include "permissions.h"

#include <map>
#include <set>
#include <string>
#include <vector>
#include <utility>
#include <Qt/qicon.h>
#include <Qt/qmainwindow.h>

class QCheckBox;
class Q3ListViewItem;
class QTimer;
class Q3FileDialog;
class Submit;
class ClarificationRequestItem;
class ClarificationItem;
class SubmissionItem;
class StandingItem;

typedef struct {
	int prio_level;
	QString msg;
} LogMessage;

class MainWindow : public QMainWindow, public Ui::MainWindowBase {
	Q_OBJECT

private:
	QIcon quietIcon, alertIcon;

	LoginDialog _login_dialog;
	Submit *_submit_dialog;
	Q3FileDialog *_submit_file_dialog;

	ServerConnection _server_con;

	PermissionSet _active_permissions;
	std::string _active_user;

	std::set<uint32_t> _subscribed_problems;

	/* Clock-related stuff */
	QTimer *timer;
	time_t projected_stop;

	/* Look up internal data by ID */
	std::map<uint32_t, ClarificationRequestItem *> clarificationRequestMap;
	std::map<uint32_t, ClarificationItem *> clarificationMap;
	std::map<uint32_t, SubmissionItem *> submissionMap;
	std::map<uint32_t, StandingItem *> standingMap;

	void switchPermissions(const std::vector<std::string> &permissions);

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
	void clarificationRequest(uint32_t submission_id, uint32_t prob_id);

protected:
	virtual void doHelpAbout();
	virtual void doFileConnect();
	virtual void doFileDisconnect();
	virtual void doForceRefresh();
	virtual void doAdminCreateUser();
	virtual void doAdminCreateGroup();
	virtual void doAdminProblemConfig();
	virtual void doAdminStartStop();
	virtual void doSubmit();
	virtual void doRequestSubmissionClarification();
	virtual void doClarificationRequest();
	virtual void doShowClarificationRequest(Q3ListViewItem*);
	virtual void doShowClarificationReply(Q3ListViewItem*);
	virtual void doTimer();
	virtual void tabChanged(QWidget*);
	virtual void doChangePassword();
	virtual void doJudgeSubscribeToProblems();
	virtual void submissionHandler(Q3ListViewItem *);
	virtual void toggleBalloonPopups(bool);
	virtual void sortStandings();
	virtual void refilterSubmissions();

	virtual void customEvent(QEvent *ev);
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

	virtual void updateStandings() { updateStandings(NULL); }
	virtual void updateSubmissions() { updateSubmissions(NULL); }

signals:
	void signalAuthControls(bool);
	void signalSubmitControls(bool);
	void signalClarificationRequestControls(bool);
	void signalClarificationReplyControls(bool);
	void signalChangePasswordControls(bool);
	void signalInStandingsControls(bool);
	void signalSeeStandingsControls(bool);
	void signalSeeAllStandingsControls(bool);
	void signalSeeFinalStandingsControls(bool);
	void signalSeeAllClarificationRequestsControls(bool);
	void signalSeeAllClarificationsControls(bool);
	void signalSeeAllSubmissionsControls(bool);
	void signalSeeAllProblemsControls(bool);
	void signalSeeProblemDetailsControls(bool);
	void signalSeeSubmissionDetailsControls(bool);
	void signalSeeUserIdControls(bool);
	void signalMarkControls(bool);
	void signalJudgeControls(bool);
	void signalJudgeOverrideControls(bool);
	void signalUserAdminControls(bool);
	void signalProblemAdminControls(bool);
	void signalServerAdminControls(bool);
	void signalStartStopControls(bool);

};

#endif
