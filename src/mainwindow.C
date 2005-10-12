#include "mainwindow.h"
#include "ui_aboutdialog.h"

#include "config.h"
#include "logger.h"
#include "ui_adduser.h"
#include "ui_submit.h"
#include "problemconfig.h"
#include "guievent.h"
#include "messageblock.h"

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
	GUIEvent *ev = new MainWindowCaller(&MainWindow::updateSubmissions);
	ev->post();
}

static void updateClarificationsFunctor(const MessageBlock*, void*) {
	GUIEvent *ev = new MainWindowCaller(&MainWindow::updateSubmissions);
	ev->post();
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

				_server_con.registerEventCallback("submission", updateSubmissionsFunctor, NULL);
				_server_con.registerEventCallback("standings", updateStandingsFunctor, NULL);
				_server_con.registerEventCallback("clarifications", updateClarificationsFunctor, NULL);
				_server_con.registerEventCallback("clarificationrequests", updateClarificationRequestsFunctor, NULL);
				_server_con.registerEventCallback("msg", event_msg, NULL);
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

	switchType("");
}

void MainWindow::doChangePassword() {
    _change_password_dialog.password->setText("");
    _change_password_dialog.confirm->setText("");
    if (_change_password_dialog.exec()) {
	std::string password = _change_password_dialog.password->text();
	if (password != _change_password_dialog.confirm->text()) {
	    QMessageBox("Password mismatch", "The two passwords entered must be the same!",
			QMessageBox::Critical, QMessageBox::Ok,
			QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
	    return;
	}
	if (_server_con.changePassword(password))
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
	NOT_IMPLEMENTED();
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
		
		item->setText(0, time_buffer);
		item->setText(1, (*l)["problem"]);
		item->setText(2, (*l)["result"]);

		for(a = l->begin(); a != l->end(); ++a)
			log(LOG_DEBUG, "%s = %s", a->first.c_str(), a->second.c_str());
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
		
		item->setText(0, time_buffer);
		item->setText(1, (*l)["problem"]);
		item->setText(2, (*l)["question"]);
		item->setText(3, (*l)["answer"]);

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
		
		item->setText(0, time_buffer);
		item->setText(1, (*l)["problem"]);
		item->setText(2, (*l)["question"]);
		item->setText(3, (*l)["status"]);

		for(a = l->begin(); a != l->end(); ++a)
			log(LOG_DEBUG, "%s = %s", a->first.c_str(), a->second.c_str());
	}
}
