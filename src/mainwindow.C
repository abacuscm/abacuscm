#include "mainwindow.h"
#include "ui_aboutdialog.h"

#include "config.h"
#include "logger.h"
#include "ui_adduser.h"
#include "ui_submit.h"
#include "ui_clarificationrequest.h"
#include "ui_viewclarificationrequest.h"
#include "ui_viewclarificationreply.h"
#include "ui_changepassworddialog.h"
#include "ui_problemsubscription.h"
#include "viewclarificationrequestsub.h"
#include "problemconfig.h"
#include "guievent.h"
#include "messageblock.h"
#include "ui_judgedecisiondialog.h"

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

#include <time.h>

using namespace std;

/********************** Notification Dialogs ************************/
class NotifyEvent : public GUIEvent {
private:
	QString _caption;
	QString _text;
	QMessageBox::Icon _icon;
public:
	NotifyEvent(const QString& caption, const QString& text, QMessageBox::Icon icon);

	virtual void process(QWidget*);
};

NotifyEvent::NotifyEvent(const QString& caption, const QString& text, QMessageBox::Icon icon) {
	_caption = caption;
	_text = text;
	_icon = icon;
}

void NotifyEvent::process(QWidget *parent) {
	QMessageBox(_caption, _text, _icon, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton, parent).exec();
}

static void window_log(int priority, const char* format, va_list ap) {
	QString caption;
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

/********************* MainWindowCaller *****************************/
typedef void (MainWindow::*MainWindowFuncPtr)();
class MainWindowCaller : public GUIEvent {
private:
	MainWindowFuncPtr _func;
public:
	MainWindowCaller(MainWindowFuncPtr func);

	virtual void process(QWidget*);
};

MainWindowCaller::MainWindowCaller(MainWindowFuncPtr func) {
	_func = func;
}

void MainWindowCaller::process(QWidget* wid) {
	MainWindow* mainwin = dynamic_cast<MainWindow*>(wid);
	if(mainwin)
		(mainwin->*_func)();
	else
		log(LOG_ERR, "MainWindowCaller::process must receive an instance of MainWindow as parent!");
}

// Would've prefered a single function here ...
static void updateStandingsFunctor(const MessageBlock*, void*) {
	GUIEvent *ev = new MainWindowCaller(&MainWindow::updateStandings);
	ev->post();
}

static void updateSubmissionsFunctor(const MessageBlock* mb, void*) {
	GUIEvent *ev = new MainWindowCaller(&MainWindow::updateSubmissions);
	ev->post();

	NotifyEvent *ne = new NotifyEvent("Submission", (*mb)["msg"], QMessageBox::NoIcon);
	ne->post();
}

static void updateClarificationRequestsFunctor(const MessageBlock*, void*) {
	GUIEvent *ev = new MainWindowCaller(&MainWindow::updateClarificationRequests);
	ev->post();
}

static void updateClarificationsFunctor(const MessageBlock*, void*) {
	GUIEvent *ev = new MainWindowCaller(&MainWindow::updateClarifications);
	ev->post();
}

static void contestStartStop(const MessageBlock* mb, void*) {
	// TODO: Something with the clock ...
	NotifyEvent *ne = new NotifyEvent("Contest Status", string("The contest has been ") + 
			(((*mb)["action"] == "start") ? "started" : "stopped"), QMessageBox::NoIcon);
	ne->post();
}

/************************** MainWindow ******************************/
MainWindow::MainWindow() {
	Config &config = Config::getConfig();

	_login_dialog.serverName->setText(config["server"]["address"]);
	_login_dialog.service->setText(config["server"]["service"]);

	GUIEvent::setReceiver(this);
	register_log_listener(window_log);
}

MainWindow::~MainWindow() {
}

void MainWindow::triggerType(std::string type, bool status) {
	if(type == "admin")
		signalAdminControls(status);
	else if(type == "judge")
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

				_server_con.registerEventCallback("submission", updateSubmissionsFunctor, NULL);
				_server_con.registerEventCallback("standings", updateStandingsFunctor, NULL);
				_server_con.registerEventCallback("updateclarifications", updateClarificationsFunctor, NULL);
				_server_con.registerEventCallback("updateclarificationrequests", updateClarificationRequestsFunctor, NULL);
				_server_con.registerEventCallback("msg", event_msg, NULL);
				_server_con.registerEventCallback("startstop", contestStartStop, NULL);

				_server_con.subscribeTime();
			} else {
				QMessageBox("Auth error", "Invalid username and/or password",
						QMessageBox::Information, QMessageBox::Ok,
						QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
				_server_con.disconnect();
			}

		} else
			QMessageBox("Connection error", "Error connecting to the server",
					QMessageBox::Critical, QMessageBox::Ok, QMessageBox::NoButton,
					QMessageBox::NoButton, this).exec();
	}
}

void MainWindow::doFileDisconnect() {
	if(!_server_con.disconnect())
		return;

	fileConnectAction->setEnabled(true);
	fileDisconnectAction->setEnabled(false);
	changePasswordAction->setEnabled(false);
	clarifyButton->setEnabled(false);

	switchType("");
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
			QMessageBox("Failed to change password!", "Something went wront while changing password...",
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
	if(prob_desc == "")
		return;

	if(!prob_conf.setProblemDescription(prob_desc)) {
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

		if(!_server_con.setProblemAttributes(0, prob_type, normal, files))
			QMessageBox::critical(this, "Error", "Server failed to accept the set values.", "O&k");
	}
}

void MainWindow::doSubmit() {
	vector<ProblemInfo> probs = _server_con.getProblems();
	if(probs.empty()) {
		QMessageBox::critical(this, "Error", "There are no active problems to submit solutions for!", "O&k");
		return;
	}

	Submit submit;

	// This code is here because Qt messes up.
	QFileDialog* fileDiag = new QFileDialog(&submit);

	connect(submit.browse, SIGNAL( clicked() ),
			fileDiag, SLOT( exec() ));
	connect(fileDiag, SIGNAL( fileSelected(const QString&) ),
			submit.filename, SLOT( setText(const QString&) ));

	vector<ProblemInfo>::iterator i;
	for(i = probs.begin(); i != probs.end(); ++i) {
		submit.problemSelection->insertItem(i->code + ": " + i->name);
	}

	if(submit.exec()) {
		// Do _NOT_ drop the ::s here - Qt has at least a close() function
		// that overloads this and causes extremely weird errors.
		int fd = ::open(submit.filename->text().ascii(), O_RDONLY);
		if(fd < 0) {
			QMessageBox::critical(this, "Error", "Unable to open specified file for reading - not submitting!", "O&k");
			return;
		}

		uint32_t prob_id = probs[submit.problemSelection->currentItem()].id;
		std::string lang = submit.language->currentText();

		_server_con.submit(prob_id, fd, lang);
		::close(fd);
	}
}

void MainWindow::doClarificationRequest() {
	vector<ProblemInfo> probs = _server_con.getProblems();

	ClarificationRequest cr;

	vector<ProblemInfo>::iterator i;
	cr.problemSelection->insertItem("General");
	for(i = probs.begin(); i != probs.end(); ++i) {
		cr.problemSelection->insertItem(i->code + ": " + i->name);
	}

	if(cr.exec()) {
		int item = cr.problemSelection->currentItem() - 1;
		uint32_t prob_id = (item == -1) ? 0 : probs[item].id;

		_server_con.clarificationRequest(prob_id, cr.question->text());
	}
}

void MainWindow::doShowClarificationRequest(QListViewItem *item) {
	ViewClarificationRequestSub vcr(atol(item->text(0)), &_server_con);

	if (_active_type == "contestant")
	    vcr.reply->setEnabled(false);
	vcr.problem->setText(item->text(2));
	vcr.question->setText(item->text(3));
	vcr.exec();
}

void MainWindow::doShowClarificationReply(QListViewItem *item) {
	ViewClarificationReply vcr;

	vcr.problem->setText(item->text(2));
	vcr.question->setText(item->text(3));
	vcr.answer->setText(item->text(4));
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

void MainWindow::updateStandings() {
	Grid data = _server_con.getStandings();

	if(data.empty())
		return;

	standings->clear();
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
		sprintf(bfr, "% 4d", ++pos);
		item->setText(0, bfr);

		int c = 1;
		for(cell = row->begin(); cell != row->end(); ++cell, ++c)
			item->setText(c, *cell);
	}
}

void MainWindow::updateSubmissions() {
	SubmissionList list = _server_con.getSubmissions();

	submissions->clear();

	SubmissionList::iterator l;
	for(l = list.begin(); l != list.end(); ++l) {
		log(LOG_DEBUG, "submission:");
		AttributeMap::iterator a;
		QListViewItem *item = new QListViewItem(submissions);
		char time_buffer[64];
		struct tm time_tm;
		time_t time;

		time = atol((*l)["time"].c_str());
		localtime_r(&time, &time_tm);
		strftime(time_buffer, sizeof(time_buffer) - 1, "%X", &time_tm);

        item->setText(0, (*l)["submission_id"]);
		item->setText(1, time_buffer);
		item->setText(2, (*l)["problem"]);
        item->setText(3, (*l)["comment"]);
	}
}

void MainWindow::judgeSubmissionHandler(QListViewItem *item) {
    if (_active_type == "judge") {
        JudgeDecisionDialog judgeDecisionDialog;
        uint32_t submission_id = strtoll(item->text(0), NULL, 0);
        uint32_t fileCount = _server_con.countMarkFiles(submission_id);
        log(LOG_DEBUG, "Submission %d has %d mark files\n", submission_id, fileCount);
        for (map<string, char *>::iterator i = judgeDecisionDialog.data.begin(); i != judgeDecisionDialog.data.end(); i++)
            delete[] i->second;
        judgeDecisionDialog.data.clear();
        while (judgeDecisionDialog.FileSelector->count() > 0)
            judgeDecisionDialog.FileSelector->removeItem(0);
        judgeDecisionDialog.FileSelector->insertItem("Select file to view...");
        for (uint32_t index = 0; index < fileCount; index++) {
            string name;
            uint32_t length;
            char *fdata;
            bool result = _server_con.getMarkFile(submission_id, index, name, (void **) &fdata, length);
            if (!result)
                log(LOG_DEBUG, "Uh-oh, something went wrong when getting file from server");
            judgeDecisionDialog.data[name] = fdata;
            judgeDecisionDialog.FileSelector->insertItem(name.c_str());
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
            if (_server_con.mark(submission_id, CORRECT, "Correct answer", AttributeMap()))
                log(LOG_INFO, "Judge marked submission %u as correct", submission_id);
            return;
        case 3:
            // wrong
            // need to mark the problem as being wrong
            if (_server_con.mark(submission_id, WRONG, "Wrong answer", AttributeMap()))
                log(LOG_INFO, "Judge marked submission %u as wrong", submission_id);
            return;
        }
    }
}

void MainWindow::updateClarifications() {
	ClarificationList list = _server_con.getClarifications();

	clarifications->clear();

	ClarificationList::iterator l;
	for(l = list.begin(); l != list.end(); ++l) {
		log(LOG_DEBUG, "clarification:");
		AttributeMap::iterator a;
		QListViewItem *item = new QListViewItem(clarifications);
		char time_buffer[64];
		struct tm time_tm;
		time_t time;

		time = atol((*l)["time"].c_str());
		localtime_r(&time, &time_tm);
		strftime(time_buffer, sizeof(time_buffer) - 1, "%X", &time_tm);
		
		item->setText(0, (*l)["id"]);
		item->setText(1, time_buffer);
		item->setText(2, (*l)["problem"]);
		item->setText(3, (*l)["question"]);
		item->setText(4, (*l)["answer"]);

		for(a = l->begin(); a != l->end(); ++a)
			log(LOG_DEBUG, "%s = %s", a->first.c_str(), a->second.c_str());
	}
}

void MainWindow::updateClarificationRequests() {
	ClarificationRequestList list = _server_con.getClarificationRequests();

	clarificationRequests->clear();

	ClarificationRequestList::iterator l;
	for(l = list.begin(); l != list.end(); ++l) {
		log(LOG_DEBUG, "clarification request:");
		AttributeMap::iterator a;
		QListViewItem *item = new QListViewItem(clarificationRequests);
		char time_buffer[64];
		struct tm time_tm;
		time_t time;

		time = atol((*l)["time"].c_str());
		localtime_r(&time, &time_tm);
		strftime(time_buffer, sizeof(time_buffer) - 1, "%X", &time_tm);
		
		item->setText(0, (*l)["id"]);
		item->setText(1, time_buffer);
		item->setText(2, (*l)["problem"]);
		item->setText(3, (*l)["question"]);
		item->setText(4, (*l)["status"]);

		for(a = l->begin(); a != l->end(); ++a)
			log(LOG_DEBUG, "%s = %s", a->first.c_str(), a->second.c_str());
	}
}
