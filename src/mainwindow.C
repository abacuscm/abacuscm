/**
 * Copyright (c) 2005 - 2006, 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "mainwindow.h"
#include "ui_aboutdialog.h"

#include "acmconfig.h"
#include "dbcon.h"
#include "logger.h"
#include "ui_adduser.h"
#include "ui_submit.h"
#include "ui_clarificationrequest.h"
#include "ui_viewclarificationrequest.h"
#include "ui_viewclarificationreply.h"
#include "ui_changepassworddialog.h"
#include "ui_problemsubscription.h"
#include "ui_startstopdialog.h"
#include "viewclarificationrequestsub.h"
#include "problemconfig.h"
#include "guievent.h"
#include "messageblock.h"
#include "ui_judgedecisiondialog.h"
#include "ui_compileroutputdialog.h"
#include "misc.h"

#include <qlineedit.h>
#include <qmessagebox.h>
#include <qaction.h>
#include <qcombobox.h>
#include <qinputdialog.h>
#include <qstring.h>
#include <fcntl.h>
#include <unistd.h>
#include <qfiledialog.h>
#include <qpushbutton.h>
#include <qlistview.h>
#include <qtextedit.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qlayout.h>
#include <qcombobox.h>
#include <qlabel.h>
#include <qtextbrowser.h>
#include <qtimer.h>
#include <qtabwidget.h>

#include <time.h>
#include <set>
#include <string>
#include <sstream>
#include <cassert>

using namespace std;

/********************** Notification Dialogs ************************/
class NotifyEvent : public GUIEvent {
private:
	string _caption;
	string _text;
	QMessageBox::Icon _icon;
public:
	NotifyEvent(const string& caption, const string& text, QMessageBox::Icon icon);

	virtual void process(QWidget*);
};

NotifyEvent::NotifyEvent(const string& caption, const string& text, QMessageBox::Icon icon) {
	_caption = caption;
	_text = text;
	_icon = icon;
}

void NotifyEvent::process(QWidget *parent) {
	QMessageBox(_caption, _text, _icon, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton, parent).exec();
}

static void window_log(int priority, const char* format, va_list ap) {
	string caption;
	QMessageBox::Icon icon;

	switch(priority) {
	case LOG_INFO:
		caption = "Info Message";
		icon = QMessageBox::Information;
		break;
	case LOG_WARNING:
		caption = "Warning";
		icon = QMessageBox::Warning;
		break;
	case LOG_ERR:
	case LOG_CRIT:
		caption = "Error";
		icon = QMessageBox::Critical;
		break;
	default:
		return;
	}

	char *msg;
	vasprintf(&msg, format, ap);

	NotifyEvent *le = new NotifyEvent(caption, msg, icon);
	le->post();

	free(msg);
}

static void event_msg(const MessageBlock* mb, void*) {
	string title = (*mb)["title"];
	string text = (*mb)["text"];

	NotifyEvent *ne = new NotifyEvent(title, text, QMessageBox::NoIcon);
	ne->post();
}

static void event_balloon(const MessageBlock *mb, void*) {
	string msg = (*mb)["contestant"] + " has solved " +
		(*mb)["problem"] + " (server: " + (*mb)["server"] + ")";

	NotifyEvent *ne = new NotifyEvent("Balloon Notification", msg, QMessageBox::Information);
	ne->post();
}

/********************** Custom list items ***************************/

#define RTTI_CLARIFICATION_REQUEST 2001
#define RTTI_CLARIFICATION         2002
#define RTTI_SUBMISSION            2003

// Returns at most one line and at most 50 characters of string, appending
// "..." if anything was cut off.
static string summary_string(const string &s)
{
	string ans = s;
	bool clipped = false;
	string::size_type nl;

	nl = ans.find('\n');
	if (nl != string::npos)
	{
		ans.erase(nl);
		clipped = true;
	}

	/* Cut off at 50, with UTF-8 awareness. Characters in 0-0x7F and 0xC0-0xFF
	 * start characters, everything else is a continuation.
	 */
	size_t cut = 0;
	int seen = 0;
	while (cut < s.size()) {
		if ((unsigned char) s[cut] < 0x80 || (unsigned char) s[cut] >= 0xC0) {
			seen++;
			if (seen > 50)
				break;
		}
		cut++;
	}
	if (cut < s.size()) {
		ans.erase(cut);
		clipped = true;
	}
	if (clipped) ans += "...";
	return ans;
}

class ClarificationRequestItem : public QListViewItem {
private:
	uint32_t _id;
	time_t _time;
	string _problem;
	bool _answered;
	string _question;

	enum Column
	{
		COLUMN_ID = 0,
		COLUMN_TIME = 1,
		COLUMN_PROBLEM = 2,
		COLUMN_ANSWERED = 3,
		COLUMN_QUESTION = 4
	};
public:
	explicit ClarificationRequestItem(QListView *parent);
	explicit ClarificationRequestItem(QListViewItem *parent);
	ClarificationRequestItem(QListView *parent, QListViewItem *after);
	ClarificationRequestItem(QListViewItem *parent, QListViewItem *after);

	uint32_t getId() const { return _id; }
	time_t getTime() const { return _time; }
	const string &getProblem() const { return _problem; }
	bool isAnswered() const { return _answered; }
	const string &getQuestion() const { return _question; }

	void setId(uint32_t id);
	void setTime(time_t time);
	void setProblem(const string &problem);
	void setAnswered(bool answered);
	void setQuestion(const string &question);

	virtual int rtti() const { return RTTI_CLARIFICATION_REQUEST; }
	virtual int compare(QListViewItem *other, int col, bool ascending) const;
};

ClarificationRequestItem::ClarificationRequestItem(QListView *parent) :
	QListViewItem(parent),
	_id(0), _time(0), _problem(""), _answered(false), _question("") {
}

ClarificationRequestItem::ClarificationRequestItem(QListViewItem *parent) :
	QListViewItem(parent),
	_id(0), _time(0), _problem(""), _answered(false), _question("") {
}

ClarificationRequestItem::ClarificationRequestItem(QListView *parent, QListViewItem *after) :
	QListViewItem(parent, after),
	_id(0), _time(0), _problem(""), _answered(false), _question("") {
}

ClarificationRequestItem::ClarificationRequestItem(QListViewItem *parent, QListViewItem *after) :
	QListViewItem(parent, after),
	_id(0), _time(0), _problem(""), _answered(false), _question("") {
}

void ClarificationRequestItem::setId(uint32_t id) {
	ostringstream s;

	_id = id;
	s << id;
	setText(COLUMN_ID, s.str());
}

void ClarificationRequestItem::setTime(time_t time) {
	struct tm time_tm;
	char time_buffer[64];

	_time = time;
	localtime_r(&time, &time_tm);
	strftime(time_buffer, sizeof(time_buffer) - 1, "%X", &time_tm);
	setText(COLUMN_TIME, time_buffer);
}

void ClarificationRequestItem::setProblem(const string &problem) {
	_problem = problem;
	setText(COLUMN_PROBLEM, QString::fromUtf8(problem.c_str()));
}

void ClarificationRequestItem::setAnswered(bool answered) {
	_answered = answered;
	setText(COLUMN_ANSWERED, answered ? "answered" : "pending");
}

void ClarificationRequestItem::setQuestion(const string &question) {
	_question = question;
	setText(COLUMN_QUESTION, QString::fromUtf8(summary_string(question).c_str()));
}

int ClarificationRequestItem::compare(QListViewItem *other, int col, bool ascending) const {
	if (other == NULL || other->rtti() != rtti())
		return QListViewItem::compare(other, col, ascending);

	const ClarificationRequestItem *i = static_cast<const ClarificationRequestItem *>(other);
	switch (col) {
	case COLUMN_ID:
		return _id < i->_id ? -1 : _id > i->_id ? 1 : 0;
	case COLUMN_TIME:
		return _time < i->_time ? -1 : _time > i->_time ? 1 : 0;
	case COLUMN_ANSWERED:
		return _answered < i->_answered ? -1 : _answered > i->_answered ? 1 : 0;
	default:
		return QListViewItem::compare(other, col, ascending);
	}
}

class ClarificationItem : public QListViewItem {
private:
	uint32_t _id;
	uint32_t _request_id;
	time_t _time;
	string _problem;
	string _question;
	string _answer;

	enum Column
	{
		COLUMN_ID = 0,
		COLUMN_TIME = 1,
		COLUMN_PROBLEM = 2,
		COLUMN_QUESTION = 3,
		COLUMN_ANSWER = 4
	};

public:
	explicit ClarificationItem(QListView *parent);
	explicit ClarificationItem(QListViewItem *parent);
	ClarificationItem(QListView *parent, QListViewItem *after);
	ClarificationItem(QListViewItem *parent, QListViewItem *after);

	uint32_t getId() const { return _id; }
	uint32_t getRequestId() const { return _request_id; }
	time_t getTime() const { return _time; }
	const string &getProblem() const { return _problem; }
	const string &getQuestion() const { return _question; }
	const string &getAnswer() const { return _answer; }

	void setId(uint32_t id);
	void setRequestId(uint32_t request_id);
	void setTime(time_t time);
	void setProblem(const string &problem);
	void setQuestion(const string &question);
	void setAnswer(const string &answer);

	virtual int rtti() const { return RTTI_CLARIFICATION; }
	virtual int compare(QListViewItem *other, int col, bool ascending) const;
};

ClarificationItem::ClarificationItem(QListView *parent) :
	QListViewItem(parent),
	_id(0), _request_id(0), _time(0), _problem(""), _question(""), _answer("") {
}

ClarificationItem::ClarificationItem(QListViewItem *parent) :
	QListViewItem(parent),
	_id(0), _request_id(0), _time(0), _problem(""), _question(""), _answer("") {
}

ClarificationItem::ClarificationItem(QListView *parent, QListViewItem *after) :
	QListViewItem(parent, after),
	_id(0), _request_id(0), _time(0), _problem(""), _question(""), _answer("") {
}

ClarificationItem::ClarificationItem(QListViewItem *parent, QListViewItem *after) :
	QListViewItem(parent, after),
	_id(0), _request_id(0), _time(0), _problem(""), _question(""), _answer("") {
}

void ClarificationItem::setId(uint32_t id) {
	ostringstream s;

	_id = id;
	s << id;
	setText(COLUMN_ID, s.str());
}

void ClarificationItem::setRequestId(uint32_t request_id) {
	_request_id = request_id;
}

void ClarificationItem::setTime(time_t time) {
	struct tm time_tm;
	char time_buffer[64];

	_time = time;
	localtime_r(&time, &time_tm);
	strftime(time_buffer, sizeof(time_buffer) - 1, "%X", &time_tm);
	setText(COLUMN_TIME, time_buffer);
}

void ClarificationItem::setProblem(const string &problem) {
	_problem = problem;
	setText(COLUMN_PROBLEM, QString::fromUtf8(problem.c_str()));
}

void ClarificationItem::setQuestion(const string &question) {
	_question = question;
	setText(COLUMN_QUESTION, QString::fromUtf8(summary_string(question).c_str()));
}

void ClarificationItem::setAnswer(const string &answer) {
	_answer = answer;
	setText(COLUMN_ANSWER, QString::fromUtf8(summary_string(answer).c_str()));
}

int ClarificationItem::compare(QListViewItem *other, int col, bool ascending) const {
	if (other == NULL || other->rtti() != rtti())
		return QListViewItem::compare(other, col, ascending);

	const ClarificationItem *i = static_cast<const ClarificationItem *>(other);
	switch (col) {
	case COLUMN_ID:
		return _id < i->_id ? -1 : _id > i->_id ? 1 : 0;
	case COLUMN_TIME:
		return _time < i->_time ? -1 : _time > i->_time ? 1 : 0;
	default:
		return QListViewItem::compare(other, col, ascending);
	}
}

class SubmissionItem : public QListViewItem {
private:
	uint32_t _id;
	time_t _contest_time;
	time_t _time;
	string _problem;
	string _status;

	/* Submissions are a little more complicated because they are filtered.
	 * QListView isn't so hot on filtering, so and doesn't allow most methods
	 * on a QListViewItem that's not in a view. So, we need to keep track
	 * internally of whether we're visible, and defer setText calls if not.
	 */
	QListView *_parent;
	bool _visible;

	enum Column {
		COLUMN_ID = 0,
		COLUMN_CONTEST_TIME = 1,
		COLUMN_TIME = 2,
		COLUMN_PROBLEM = 3,
		COLUMN_STATUS = 4
	};
public:
	SubmissionItem(QListView *parent);
	SubmissionItem(QListView *parent, QListViewItem *after);

	void setId(uint32_t id);
	void setContestTime(time_t contest_time);
	void setTime(time_t time);
	void setProblem(const string &problem);
	void setStatus(const string &status);
	void setVisible(bool visible);

	uint32_t getId() const { return _id; }
	time_t getContestTime() const { return _contest_time; }
	time_t getTime() const { return _time; }
	const string &getProblem() const { return _problem; }
	const string &getStatus() const { return _status; }
	bool isVisible() const { return _visible; }

	virtual int rtti() const { return RTTI_SUBMISSION; }
	virtual int compare(QListViewItem *other, int col, bool ascending) const;
};

SubmissionItem::SubmissionItem(QListView *parent) :
	QListViewItem(parent),
	_id(0), _contest_time(0), _time(0), _problem(""), _status(""),
	_parent(parent), _visible(true) {
}

SubmissionItem::SubmissionItem(QListView *parent, QListViewItem *after) :
	QListViewItem(parent, after),
	_id(0), _contest_time(0), _time(0), _problem(""), _status(""),
	_parent(parent), _visible(true) {
}

void SubmissionItem::setId(uint32_t id) {
	_id = id;
	if (_visible) {
		ostringstream s;
		s << id;
		setText(COLUMN_ID, s.str());
	}
}

void SubmissionItem::setContestTime(time_t contest_time) {
	char buffer[64];

	_contest_time = contest_time;
	if (_visible) {
		sprintf(buffer, "%02ld:%02ld:%02ld", (long) (contest_time / 3600), (long) (contest_time / 60 % 60), (long) (contest_time % 60));
		setText(COLUMN_CONTEST_TIME, buffer);
	}
}

void SubmissionItem::setTime(time_t time) {
	struct tm time_tm;
	char time_buffer[64];

	_time = time;
	if (_visible) {
		localtime_r(&time, &time_tm);
		strftime(time_buffer, sizeof(time_buffer) - 1, "%X", &time_tm);
		setText(COLUMN_TIME, time_buffer);
	}
}

void SubmissionItem::setProblem(const string &problem) {
	_problem = problem;
	setText(COLUMN_PROBLEM, problem);
}

void SubmissionItem::setStatus(const string &status) {
	_status = status;
	if (_visible)
		setText(COLUMN_STATUS, status);
}

void SubmissionItem::setVisible(bool visible) {
	if (_visible && !visible) {
		_parent->takeItem(this);
		_visible = false;
	}
	else if (!_visible && visible) {
		_parent->insertItem(this);
		/* Trigger updates of the text */
		setId(_id);
		setContestTime(_contest_time);
		setTime(_time);
		setProblem(_problem);
		setStatus(_status);
		_visible = true;
	}
}

int SubmissionItem::compare(QListViewItem *other, int col, bool ascending) const {
	if (other == NULL || other->rtti() != rtti())
		return QListViewItem::compare(other, col, ascending);

	const SubmissionItem *i = static_cast<const SubmissionItem *>(other);
	switch (col) {
	case COLUMN_ID:
		return _id < i->_id ? -1 : _id > i->_id ? 1 : 0;
	case COLUMN_CONTEST_TIME:
		return _contest_time < i->_contest_time ? -1 : _contest_time > i->_contest_time ? 1 : 0;
	case COLUMN_TIME:
		return _time < i->_time ? -1 : _time > i->_time ? 1 : 0;
	default:
		return QListViewItem::compare(other, col, ascending);
	}
}

/********************* MainWindowCaller *****************************/
typedef void (MainWindow::*MainWindowFuncPtr)(const MessageBlock *mb);
class MainWindowCaller : public GUIEvent {
private:
	MainWindowFuncPtr _func;
public:
	MainWindowCaller(MainWindowFuncPtr func, const MessageBlock *mb = NULL);

	virtual void process(QWidget*);
};

MainWindowCaller::MainWindowCaller(MainWindowFuncPtr func, const MessageBlock *mb) : GUIEvent(mb) {
	_func = func;
}

void MainWindowCaller::process(QWidget* wid) {
	MainWindow* mainwin = dynamic_cast<MainWindow*>(wid);
	if(mainwin)
		(mainwin->*_func)(getMB());
	else
		log(LOG_ERR, "MainWindowCaller::process must receive an instance of MainWindow as parent!");
}

// Would've prefered a single function here ...
static void updateStatusFunctor(const MessageBlock*, void*) {
	GUIEvent *ev = new MainWindowCaller(&MainWindow::updateStatus);
	ev->post();
}

static void updateStandingsFunctor(const MessageBlock*, void*) {
	GUIEvent *ev = new MainWindowCaller(&MainWindow::updateStandings);
	ev->post();
}

static void updateSubmissionsFunctor(const MessageBlock* mb, void*) {
	GUIEvent *ev = new MainWindowCaller(&MainWindow::updateSubmissions);
	ev->post();

	if ((*mb)["msg"] != "") {
		NotifyEvent *ne = new NotifyEvent("Submission", (*mb)["msg"], QMessageBox::NoIcon);
		ne->post();
	}
}

static void updateClarificationRequestsFunctor(const MessageBlock* mb, void*) {
	GUIEvent *ev = new MainWindowCaller(&MainWindow::updateClarificationRequests, mb);
	ev->post();
}

static void updateClarificationsFunctor(const MessageBlock*mb, void*) {
	GUIEvent *ev = new MainWindowCaller(&MainWindow::updateClarifications, mb);
	ev->post();
}

static void contestStartStop(const MessageBlock* mb, void*) {
	// TODO: Something with the clock ...
	NotifyEvent *ne = new NotifyEvent("Contest Status", string("The contest has been ") +
			(((*mb)["action"] == "start") ? "started" : "stopped"), QMessageBox::NoIcon);
	ne->post();

	GUIEvent *ev = new MainWindowCaller(&MainWindow::updateStatus);
	ev->post();
}

static void server_disconnect(const MessageBlock*, void*) {
	NotifyEvent *ne = new NotifyEvent("Server Disconnected", "You have been disconnected from the server", QMessageBox::Critical);
	ne->post();

	GUIEvent *ev = new MainWindowCaller(&MainWindow::serverDisconnect);
	ev->post();
}
/************************** MainWindow ******************************/
MainWindow::MainWindow() {
	Config &config = Config::getConfig();

	ThreadSSL::initialise();

	_login_dialog.serverName->setText(config["server"]["address"]);
	_login_dialog.service->setText(config["server"]["service"]);

	clarifications->setSorting(1, FALSE);
	clarificationRequests->setSorting(1, FALSE);
	submissions->setSorting(1, FALSE);

	GUIEvent::setReceiver(this);
	register_log_listener(window_log);

	projected_stop = 0;
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(doTimer()));
	timer->start(1000, false); // Updates clock once per second

	_submit_dialog = NULL;
	_submit_file_dialog = NULL;
}

MainWindow::~MainWindow() {
	ThreadSSL::cleanup();
}

void MainWindow::triggerType(std::string type, bool status) {
	if(type == "admin") {
		signalAdminControls(status);
		signalJudgeControls(status);
	} else if(type == "judge")
		signalJudgeControls(status);
	else if(type == "contestant")
		signalContestantControls(status);
	else
		log(LOG_ERR, "Unknown control-type '%s'.", type.c_str());
}

void MainWindow::switchType(std::string type) {
	log(LOG_DEBUG, "Changing active type from '%s' to '%s'",
			_active_type.c_str(), type.c_str());
	if(_active_type != "")
		triggerType(_active_type, false);

	_active_type = type;

	if(_active_type != "")
		triggerType(_active_type, true);
}

void MainWindow::doTimer() {
	if (projected_stop > 0)
	{
		time_t now = time(NULL);
		time_t remain = 0;
		if (now < projected_stop)
			remain = projected_stop - now;

		char buffer[30];
		sprintf(buffer, "%02ld:%02ld:%02ld",
			(long) remain / 3600,
			(long) (remain / 60) % 60,
			(long) remain % 60);
		clock->setText(buffer);
	}
}

void MainWindow::doHelpAbout() {
	AboutDialog about;
	about.exec();
}

void MainWindow::doFileConnect() {
	_login_dialog.password->setText("");
	if(_login_dialog.exec()) {
		std::string sname = _login_dialog.serverName->text();
		std::string service = _login_dialog.service->text();
		if(_server_con.connect(sname, service)) {
			_server_con.registerEventCallback("close", server_disconnect, NULL);

			std::string uname = _login_dialog.username->text();
			std::string pass = _login_dialog.password->text();
			if(_server_con.auth(uname, pass)) {
				QMessageBox("Connected", "You are now connected to the server",
						QMessageBox::Information, QMessageBox::Ok,
						QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
				std::string type = _server_con.whatAmI();
				switchType(type);
				fileConnectAction->setEnabled(false);
				fileDisconnectAction->setEnabled(true);
				changePasswordAction->setEnabled(true);
				clarifyButton->setEnabled(true);

				_server_con.registerEventCallback("updatesubmissions", updateSubmissionsFunctor, NULL);
				_server_con.registerEventCallback("standings", updateStandingsFunctor, NULL);
				_server_con.registerEventCallback("msg", event_msg, NULL);
				_server_con.registerEventCallback("startstop", contestStartStop, NULL);
				_server_con.registerEventCallback("balloon", event_balloon, NULL);
				_server_con.registerEventCallback("updateclarificationrequests", updateClarificationRequestsFunctor, NULL);
				_server_con.registerEventCallback("updateclarifications", updateClarificationsFunctor, NULL);
				_server_con.registerEventCallback("updateclock", updateStatusFunctor, NULL);
				_server_con.subscribeTime();
				updateStatus();
				updateClarificationRequests();
				updateStandings();
				updateSubmissions();
			} else {
				_server_con.deregisterEventCallback("close", server_disconnect);
				_server_con.disconnect();
			}

		} else
			QMessageBox("Connection error", "Error connecting to the server",
					QMessageBox::Critical, QMessageBox::Ok, QMessageBox::NoButton,
					QMessageBox::NoButton, this).exec();
	}
}

void MainWindow::doFileDisconnect() {
	_server_con.deregisterEventCallback("close", server_disconnect);
	if(!_server_con.disconnect()) {
		_server_con.registerEventCallback("close", server_disconnect, NULL);
		return;
	}

	fileConnectAction->setEnabled(true);
	fileDisconnectAction->setEnabled(false);
	changePasswordAction->setEnabled(false);
	clarifyButton->setEnabled(false);

	switchType("");
	updateStatus();
	updateClarificationRequests();
	updateSubmissions();
	updateStandings();
}

void MainWindow::doChangePassword() {
	ChangePasswordDialog change_password_dialog;
	vector<UserInfo> users;

	if (_active_type == "admin")
	{
		users = _server_con.getUsers();
		for(vector<UserInfo>::iterator i = users.begin(); i != users.end(); i++)
			change_password_dialog.user->insertItem(i->username);
		change_password_dialog.user->setEnabled(true);
	}

	if (change_password_dialog.exec()) {
		std::string password = change_password_dialog.password->text();
		if (password != change_password_dialog.confirm->text()) {
			QMessageBox("Password mismatch", "The two passwords entered must be the same!",
					QMessageBox::Critical, QMessageBox::Ok,
					QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
			return;
		}

		bool result = false;
		if (_active_type == "admin")
		{
			int index = change_password_dialog.user->currentItem();
			result = _server_con.changePassword(users[index].id, password);
		}
		else
			result = _server_con.changePassword(password);

		if (result)
			QMessageBox("Password changed!", "Password successfully changed",
					QMessageBox::Information, QMessageBox::Ok,
					QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
		else
			QMessageBox("Failed to change password!", "Something went wrong while changing password...",
					QMessageBox::Critical, QMessageBox::Ok,
					QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
	}
}

void MainWindow::doAdminCreateUser() {
	AddUser add_user_dialog;

	while(add_user_dialog.exec()) {
		if(add_user_dialog.username->text() == "") {
			QMessageBox("Username verify error",
					"You cannot have a blank username",
					QMessageBox::Warning, QMessageBox::Ok,
					QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
		} else if(add_user_dialog.pass1->text() != add_user_dialog.pass2->text()) {
			QMessageBox("Password verify error",
					"The passwords you typed did not match",
					QMessageBox::Warning, QMessageBox::Ok,
					QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
		} else if(add_user_dialog.pass1->text() == "") {
			QMessageBox("Password verify error",
					"You cannot have an empty password",
					QMessageBox::Warning, QMessageBox::Ok,
					QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
		} else if(!_server_con.createuser(add_user_dialog.username->text(),
					(const char *) add_user_dialog.friendlyname->text().utf8(),
					add_user_dialog.pass1->text(),
					add_user_dialog.type->currentText().lower())) {
			QMessageBox("Connection error",
					"Error sending adduser request.",
					QMessageBox::Critical, QMessageBox::Ok,
					QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
		} else {
			break;
		}
	}
}

void MainWindow::doAdminProblemConfig() {
	ProblemConfig prob_conf(this);

	string prob = "new";
	string prob_type;

	if(prob == "new") {
		vector<string> prob_types = _server_con.getProblemTypes();
		if(prob_types.empty())
			return;

		if(prob_types.size() == 1) {
			prob_type = prob_types[0];
		} else {
			QStringList lst;
			vector<string>::iterator i;
			for(i = prob_types.begin(); i != prob_types.end(); ++i)
				lst << *i;

			bool ok;
			QString res = QInputDialog::getItem("Problem type", "Please select the type of problem", lst, 0, false, &ok, this);
			if(ok) {
				prob_type = res.ascii(); // the cast operator doesn't compile :(
			} else
				return;
		}
	} else {
		NOT_IMPLEMENTED();
		return;
	}

	string prob_desc = _server_con.getProblemDescription(prob_type);
	vector<ProblemInfo> probs = _server_con.getProblems();
	vector<string> existing_problems;
	for (size_t i = 0; i < probs.size(); i++) {
		existing_problems.push_back(probs[i].code);
	}

	if(!prob_conf.setProblemDescription(prob_desc, existing_problems)) {
		QMessageBox::critical(this, "Error", "Error initialising problem description", "O&k");
		return;
	}

	if(prob_conf.exec()) {
		AttributeMap normal;
		AttributeMap files;
		if(!prob_conf.getProblemAttributes(normal, files)) {
			QMessageBox::critical(this, "Error", "Error retrieving problem attributes that just got set", "O&k");
			return;
		}
		AttributeMap::iterator i;
		log(LOG_DEBUG, "Successful dialog, dumping attrs:");
		for(i = normal.begin(); i != normal.end(); ++i)
			log(LOG_DEBUG, "%s -> %s", i->first.c_str(), i->second.c_str());
		for(i = files.begin(); i != files.end(); ++i)
			log(LOG_DEBUG, "%s -> %s", i->first.c_str(), i->second.c_str());

		std::vector<std::string> dependencies = prob_conf.getDependencies();
		ProblemList deps;
		for (size_t i = 0; i < dependencies.size(); i++)
		{
			uint32_t id = 0;
			for (size_t j = 0; j < probs.size(); j++) {
				if (dependencies[i] == probs[j].code) {
					id = probs[j].id;
					break;
				}
			}
			deps.push_back(id);
		}

		if(!_server_con.setProblemAttributes(0, prob_type, normal, files, deps))
			QMessageBox::critical(this, "Error", "Server failed to accept the set values.", "O&k");
	}
}

void MainWindow::doAdminStartStop() {
	StartStopDialog dialog;
	struct tm start_tm, stop_tm;
	time_t start, stop, now;
	bool correct = false;

	now = time(NULL);
	localtime_r(&now, &start_tm);
	localtime_r(&now, &stop_tm);
	while (!correct)
	{
		if (dialog.exec())
		{
			string start_str, stop_str;
			char *err;
			correct = true;

			start_str = (const char *) dialog.start->text();
			stop_str = (const char *) dialog.stop->text();
			err = strptime(start_str.c_str(), "%T", &start_tm);
			if (!err || *err || start_str == "")
				correct = false;
			err = strptime(stop_str.c_str(), "%T", &stop_tm);
			if (!err || *err || stop_str == "")
				correct = false;

			start = mktime(&start_tm);
			stop = mktime(&stop_tm);
			correct = correct && start < stop && start != (time_t) -1 && stop != (time_t) -1;
		}
		else
			return;

		if (!correct)
			QMessageBox("Error", "Please enter start and end times as HH:MM:SS",
					QMessageBox::Warning, QMessageBox::Ok,
					QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
	}

		bool global = dialog.server->currentItem() == 0;
	_server_con.startStop(global, true, start)
	&& _server_con.startStop(global, false, stop);
}

void MainWindow::doSubmit() {
	vector<ProblemInfo> probs = _server_con.getSubmissibleProblems();
	if(probs.empty()) {
		QMessageBox::critical(this, "Error", "There are no active problems to submit solutions for!", "O&k");
		return;
	}

	if (!_submit_dialog)
	{
		_submit_dialog = new Submit(this);
	}
	if (!_submit_file_dialog)
	{
		_submit_file_dialog = new QFileDialog(_submit_dialog);
		connect(_submit_dialog->browse, SIGNAL( clicked() ),
			_submit_file_dialog, SLOT( exec() ));
		connect(_submit_file_dialog, SIGNAL( fileSelected(const QString&) ),
			_submit_dialog->filename, SLOT( setText(const QString&) ));
	}

	QString old_text = _submit_dialog->problemSelection->currentText();
	int new_item = 0;

	_submit_dialog->problemSelection->clear();
	vector<ProblemInfo>::iterator i;
	for(i = probs.begin(); i != probs.end(); ++i) {
		QString text = i->code + ": " + i->name;
		_submit_dialog->problemSelection->insertItem(text);
		if (text == old_text) new_item = i - probs.begin();
	}
	_submit_dialog->problemSelection->setCurrentItem(new_item);

	if(_submit_dialog->exec()) {
		// Do _NOT_ drop the ::s here - Qt has at least a close() function
		// that overloads this and causes extremely weird errors.
		int fd = ::open(_submit_dialog->filename->text().ascii(), O_RDONLY);
		if(fd < 0) {
			QMessageBox::critical(this, "Error", "Unable to open specified file for reading - not submitting!", "O&k");
			return;
		}

		uint32_t prob_id = probs[_submit_dialog->problemSelection->currentItem()].id;
		std::string lang = _submit_dialog->language->currentText();

		_server_con.submit(prob_id, fd, lang);
		::close(fd);
	}
}

void MainWindow::doClarificationRequest() {
	vector<ProblemInfo> probs = _server_con.getSubmissibleProblems();

	ClarificationRequest cr;

	vector<ProblemInfo>::iterator i;
	cr.problemSelection->insertItem("General");
	for(i = probs.begin(); i != probs.end(); ++i) {
		cr.problemSelection->insertItem(i->code + ": " + i->name);
	}

	if(cr.exec()) {
		int item = cr.problemSelection->currentItem() - 1;
		uint32_t prob_id = (item == -1) ? 0 : probs[item].id;

		_server_con.clarificationRequest(prob_id, (const char *) cr.question->text().utf8());
	}
}

void MainWindow::doShowClarificationRequest(QListViewItem *item) {
	ViewClarificationRequestSub vcr(atol(item->text(0)), &_server_con);

	if (_active_type == "contestant")
		vcr.reply->setEnabled(false);
	assert(item->rtti() == RTTI_CLARIFICATION_REQUEST);
	const ClarificationRequestItem *cri = static_cast<const ClarificationRequestItem *>(item);
	vcr.problem->setText(QString::fromUtf8(cri->getProblem().c_str()));
	vcr.question->setText(QString::fromUtf8(cri->getQuestion().c_str()));
	vcr.exec();
}

void MainWindow::doShowClarificationReply(QListViewItem *item) {
	ViewClarificationReply vcr;

	assert(item->rtti() == RTTI_CLARIFICATION);
	const ClarificationItem *ci = static_cast<const ClarificationItem *>(item);
	vcr.problem->setText(QString::fromUtf8(ci->getProblem().c_str()));
	vcr.question->setText(QString::fromUtf8(ci->getQuestion().c_str()));
	vcr.answer->setText(QString::fromUtf8(ci->getAnswer().c_str()));
	vcr.exec();
}

void MainWindow::doJudgeSubscribeToProblems() {
	ProblemSubscriptionDialog problem_subscription_dialog;
	std::vector<QCheckBox *> problem_subscription_buttons;

	// first, get a list of all problems
	std::vector<ProblemInfo> problems = _server_con.getProblems();
	if(problems.empty()) {
		QMessageBox::critical(this, "Error", "There are no active problems to submit solutions for!", "O&k");
		return;
	}

	// then a list of all problems that this judge is
	// subscribed to
	std::vector<bool> subscribed = _server_con.getSubscriptions(problems);

	QVBoxLayout *l = new QVBoxLayout(problem_subscription_dialog.problemButtons);

	for (unsigned int p = 0; p < problems.size(); p++) {
		QCheckBox *button = new QCheckBox(
				problems[p].name.c_str(),
				problem_subscription_dialog.problemButtons);
		l->addWidget(button);
		button->setChecked(subscribed[p]);
		problem_subscription_buttons.push_back(button);
	}

	QSpacerItem *spacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
	l->addItem( spacer );

	if (problem_subscription_dialog.exec()) {
		bool changes = false;
		bool failure = false;
		for (unsigned int p = 0; p < problems.size(); p++) {
			if (problem_subscription_buttons[p]->isChecked() != subscribed[p]) {
				// effect the change
				bool result;
				if (subscribed[p])
					result = _server_con.unsubscribeToProblem(problems[p]);
				else
					result = _server_con.subscribeToProblem(problems[p]);
				failure = failure | (!result);
				changes = true;
			}
		}
		if (changes) {
			if (failure)
				QMessageBox("Error changing subscription!", "An error occurred whilst changing your subscription settings",
						QMessageBox::Critical, QMessageBox::Ok,
						QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
			else
				QMessageBox("Subscriptions changed!", "Your subscription settings have been changed",
						QMessageBox::Information, QMessageBox::Ok,
						QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
			updateSubmissions();
		}
	}
}

void MainWindow::tabChanged(QWidget* newtab) {
	if(newtab == tabSubmissions) {
		updateSubmissions();
	} else if(newtab == tabStandings) {
		updateStandings();
	} else if(newtab == tabClarifications) {
		updateClarifications();
	} else if(newtab == tabClarificationRequests) {
		updateClarificationRequests();
	}
}

void MainWindow::customEvent(QCustomEvent *ev) {
	GUIEvent *guiev = dynamic_cast<GUIEvent*>(ev);
	if(guiev)
		guiev->process(this);
	else
		log(LOG_DEBUG, "Unknown QCustomEvent!!!");
}

void MainWindow::updateStatus(const MessageBlock *) {
	if (_active_type == "")
	{
		status->setText("Not connected");
		projected_stop = 0;
		clock->setText("");
	} else if (_server_con.contestRunning()) {
		time_t remain = _server_con.contestRemain();
		time_t now = time(NULL);
		projected_stop = remain + now;
		status->setText("Contest running");
		clock->setText("");  // timer event will update it
	} else {
		status->setText("Contest stopped");
		projected_stop = 0;
		clock->setText("");
	}
}

void MainWindow::updateStandings(const MessageBlock *) {
	if (maintabs->currentPage() != tabStandings)
		return;

	// keep in mind that a real contestant can't get non-contestant standings,
	// so passing true in those cases has no effect.
	Grid data = _server_con.getStandings(_active_type == "contestant" || nonContestInStandingsAction->isOn());

	standings->clear();
	if(data.empty())
		return;

	while(standings->columns())
		standings->removeColumn(0);

	Grid::iterator row = data.begin();
	list<string>::iterator cell;

	standings->addColumn("Pos");
	for(cell = row->begin(); cell != row->end(); ++cell)
		standings->addColumn(*cell);

	int pos = 0;
	while(++row != data.end()) {
		QListViewItem *item = new QListViewItem(standings);

		char bfr[20];
		sprintf(bfr, "%02d", ++pos);
		item->setText(0, bfr);

		int c = 1;
		for(cell = row->begin(); cell != row->end(); ++cell, ++c)
			item->setText(c, QString::fromUtf8(cell->c_str()));
	}
}

void MainWindow::updateSubmissions(const MessageBlock *mb) {
	if (maintabs->currentPage() != tabSubmissions)
		return;

	set<int> wanted_states;
	bool filter = (_active_type == "judge" || _active_type == "admin") && judgesShowOnlySubscribedSubmissionsAction->isOn();
	std::map<string, bool> is_subscribed;
	std::vector<ProblemInfo> problems;

	if (_active_type != "") { // first check that we're connected
		problems = _server_con.getProblems();
		if (filter) {
			std::vector<bool> subscribed = _server_con.getSubscriptions(problems);
			for (unsigned int p = 0; p < problems.size(); p++)
				is_subscribed[problems[p].code] = subscribed[p];

				if (stateFilterUnmarkedAction->isOn())
					wanted_states.insert(PENDING);

				if (stateFilterJudgeAction->isOn())
					wanted_states.insert(JUDGE);

				if (stateFilterWrongAction->isOn())
					wanted_states.insert(WRONG);

				if (stateFilterCorrectAction->isOn())
					wanted_states.insert(CORRECT);

				if (stateFilterTimeExceededAction->isOn())
					wanted_states.insert(TIME_EXCEEDED);

				if (stateFilterCompileAction->isOn())
					wanted_states.insert(COMPILE_FAILED);

				if (stateFilterAbnormalAction->isOn())
					wanted_states.insert(ABNORMAL);

				if (stateFilterOtherAction->isOn())
					wanted_states.insert(OTHER);
		}
	}

	// TODO: remove the if true and do an incremental else branch
	if (true || mb == NULL || !mb->hasAttribute("submission_id")) {
		/* Full update, or a legacy message that hasn't been updated */
		SubmissionList list = _server_con.getSubmissions();

		/* Clear out the list, included anything filtered away */
		map<uint32_t, SubmissionItem *>::iterator i;
		for (i = submissionMap.begin(); i != submissionMap.end(); ++i)
			delete i->second;
		submissionMap.clear();

		SubmissionList::iterator l;
		for(l = list.begin(); l != list.end(); ++l) {
			bool visible = true;
			if (filter) {
				std::map<string, bool>::iterator s = is_subscribed.find((*l)["problem"]);
				if(s == is_subscribed.end())
					log(LOG_WARNING, "Couldn't find %s in subscription map", (*l)["problem"].c_str());
				else
					if (!s->second)
						// not subscribed to this problem
						continue;
				int result = atol((*l)["result"].c_str());
				if (wanted_states.find(result) == wanted_states.end())
					visible = false;
			}

			SubmissionItem *item = new SubmissionItem(submissions);
			uint32_t id = strtoul((*l)["submission_id"].c_str(), NULL, 10);
			submissionMap[id] = item;

			item->setVisible(visible);
			item->setId(id);
			item->setContestTime(atoll((*l)["contesttime"].c_str()));
			item->setTime(atoll((*l)["time"].c_str()));
			item->setProblem((*l)["problem"]);
			item->setStatus((*l)["comment"]);
		}
	}
}

void MainWindow::submissionHandler(QListViewItem *item) {
	if (_active_type == "judge" || _active_type == "admin") {
		JudgeDecisionDialog judgeDecisionDialog;
		uint32_t submission_id = strtoll(item->text(0), NULL, 0);
		uint32_t fileCount = _server_con.countMarkFiles(submission_id);
		log(LOG_DEBUG, "Submission %d has %d mark files\n", submission_id, fileCount);
		judgeDecisionDialog.data.clear();
		while (judgeDecisionDialog.FileSelector->count() > 0)
			judgeDecisionDialog.FileSelector->removeItem(0);
		judgeDecisionDialog.FileSelector->insertItem("Select file to view...");
		for (uint32_t index = 0; index < fileCount; index++) {
			string name;
			uint32_t length;
			union {
				char *fdata;
				void *vdata;
			};
			bool result = _server_con.getMarkFile(submission_id, index, name, &vdata, length);
			if (!result)
				log(LOG_DEBUG, "Uh-oh, something went wrong when getting file from server");
			judgeDecisionDialog.data[name] = string(fdata, length);
			judgeDecisionDialog.FileSelector->insertItem(name.c_str());
			delete[] fdata;
		}

		// fetch expected output
		vector<ProblemInfo> problems = _server_con.getProblems();
		uint32_t problemId = 0;
		for (uint32_t p = 0; p < problems.size(); p++)
			if (item->text(3) == problems[p].code) {
				problemId = problems[p].id;
				break;
			}
		if (problemId != 0) {
			// now add other files
			char *expectedOutput;
			uint32_t expectedOutputLength;
			if (_server_con.getProblemFile(problemId, "testcase.output", &expectedOutput, &expectedOutputLength)) {
				// got it
				judgeDecisionDialog.data["Expected output"] = string(expectedOutput, expectedOutputLength);
				delete[] expectedOutput;
				judgeDecisionDialog.FileSelector->insertItem("Expected output");
			}
		}

		// and now fetch contestant's source
		char *contestantSource;
		uint32_t contestantSourceLength;
		if (_server_con.getSubmissionSource(submission_id, &contestantSource, &contestantSourceLength)) {
			// got it
			judgeDecisionDialog.data["Contestant source"] = string(contestantSource, contestantSourceLength);
			delete[] contestantSource;
			judgeDecisionDialog.FileSelector->insertItem("Contestant source");
		}

		int result = judgeDecisionDialog.exec();
		switch (result) {
		case 1:
			// deferred
			// in this case, we just do nothing
			log(LOG_INFO, "Judge deferred marking of submission %u", submission_id);
			return;
		case 2:
			// correct
			// need to mark the problem as being correct
			if (_server_con.mark(submission_id, CORRECT, "Correct answer", AttributeMap())) {
				log(LOG_INFO, "Judge marked submission %u as correct", submission_id);
				updateSubmissions();
			}
			return;
		case 3:
			// wrong
			// need to mark the problem as being wrong
			if (_server_con.mark(submission_id, WRONG, "Wrong answer", AttributeMap())) {
				log(LOG_INFO, "Judge marked submission %u as wrong", submission_id);
				updateSubmissions();
			}
			return;
		}
	}
	else if (_active_type == "contestant") {
		if (item->text(4) == "Compilation failed") {
			CompilerOutputDialog compilerOutputDialog;
			string name;
			uint32_t length;
			union {
				char *data;
				void *vdata;
			};
			uint32_t submission_id = strtoll(item->text(0), NULL, 0);
			bool result = _server_con.getMarkFile(submission_id, 0, name, &vdata, length);
			if (!result) {
				log(LOG_DEBUG, "Uh-oh, something went wrong when getting file from server");
				QMessageBox("Failed to fetch compilation log", "There was a problem fetching your compilation log",
						QMessageBox::Critical, QMessageBox::Ok,
						QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
				return;
			}
			compilerOutputDialog.FileData->setText(QString::fromUtf8(data));
			delete[] data;
			compilerOutputDialog.exec();
		}
	}
}

void MainWindow::toggleBalloonPopups(bool activate) {
	if(!_server_con.watchBalloons(activate)) {
		judgesNotify_about_Correct_SubmissionsAction->setOn(false);
		//QMessageBox::critical(this, "Error", "Error subscribing to balloon notifications", "O&k");

	}
}

template<typename T>
void MainWindow::setClarification(ClarificationItem *item, T &c) {
	log(LOG_DEBUG, "clarification:");
	for(typename T::const_iterator a = c.begin(); a != c.end(); ++a)
		log(LOG_DEBUG, "%s = %s", a->first.c_str(), a->second.c_str());

	uint32_t id = strtoul(c["id"].c_str(), NULL, 10);
	uint32_t request_id = strtoul(c["req_id"].c_str(), NULL, 10);
	item->setId(id);
	item->setRequestId(request_id);
	item->setTime(strtoull(c["time"].c_str(), NULL, 10));
	item->setProblem(c["problem"]);
	item->setQuestion(c["question"]);
	item->setAnswer(c["answer"]);

	map<uint32_t, ClarificationRequestItem *>::iterator pos;
	pos = clarificationRequestMap.find(request_id);
	if (pos != clarificationRequestMap.end())
		pos->second->setAnswered(true);
}

void MainWindow::updateClarifications(const MessageBlock *mb) {
	if (mb == NULL) {
		/* Full refresh */
		ClarificationList list = _server_con.getClarifications();

		clarifications->clear();
		clarificationMap.clear();

		ClarificationList::iterator l;
		for(l = list.begin(); l != list.end(); ++l) {
			ClarificationItem *item = new ClarificationItem(clarifications);
			setClarification(item, (*l));
			clarificationMap[item->getId()] = item;
		}
	}
	else {
		/* Replace if already present, otherwise add */
		uint32_t id = strtoull((*mb)["id"].c_str(), NULL, 10);
		ClarificationItem *&item = clarificationMap[id];
		if (!item)
			item = new ClarificationItem(clarifications);

		setClarification(item, *mb);
		if (_active_type == "contestant") {
			ViewClarificationReply dialog;

			dialog.problem->setText(QString::fromUtf8(item->getProblem().c_str()));
			dialog.question->setText(QString::fromUtf8(item->getQuestion().c_str()));
			dialog.answer->setText(QString::fromUtf8(item->getAnswer().c_str()));
			dialog.exec();
		}
	}
}

template<typename T>
void MainWindow::setClarificationRequest(ClarificationRequestItem *item, T &cr) {
	log(LOG_DEBUG, "clarification request:");
	for(typename T::const_iterator a = cr.begin(); a != cr.end(); ++a)
		log(LOG_DEBUG, "%s = %s", a->first.c_str(), a->second.c_str());

	uint32_t id = strtoul(cr["id"].c_str(), NULL, 10);
	item->setId(id);
	item->setTime(strtoull(cr["time"].c_str(), NULL, 10));
	item->setProblem(cr["problem"]);
	item->setQuestion(cr["question"]);

	bool answered = false;
	/* Check if we already have a clarification */
	for (map<uint32_t, ClarificationItem *>::const_iterator i = clarificationMap.begin(); i != clarificationMap.end(); ++i)
		if (i->second->getRequestId() == id) {
			answered = true;
			break;
		}
	item->setAnswered(answered);
}

void MainWindow::updateClarificationRequests(const MessageBlock *mb) {
	if (maintabs->currentPage() != tabClarificationRequests)
		return;

	if (mb == NULL) {
		/* Full refresh */

		/* First update clarifications, since they're needed to figure out if
		 * our queries have been answered
		 */
		updateClarifications();

		ClarificationRequestList list = _server_con.getClarificationRequests();

		clarificationRequests->clear();
		clarificationRequestMap.clear();

		ClarificationRequestList::iterator l;
		for(l = list.begin(); l != list.end(); ++l) {
			ClarificationRequestItem *item = new ClarificationRequestItem(clarificationRequests);
			setClarificationRequest(item, *l);
			clarificationRequestMap[item->getId()] = item;
		}
	}
	else {
		/* Replace if already present, otherwise add */
		uint32_t id = strtoull((*mb)["id"].c_str(), NULL, 10);
		ClarificationRequestItem *&item = clarificationRequestMap[id];
		if (!item)
			item = new ClarificationRequestItem(clarificationRequests);

		setClarificationRequest(item, *mb);
	}
}

void MainWindow::serverDisconnect(const MessageBlock *) {
	doFileDisconnect();
}

std::string MainWindow::getActiveType() {
	return _active_type;
}
