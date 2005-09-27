#include "mainwindow.h"
#include "ui_aboutdialog.h"

#include "config.h"
#include "logger.h"
#include "ui_adduser.h"
#include "ui_submit.h"
#include "problemconfig.h"

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

using namespace std;

MainWindow::MainWindow() {
	Config &config = Config::getConfig();

	_login_dialog.serverName->setText(config["server"]["address"]);
	_login_dialog.service->setText(config["server"]["service"]);
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
	switchType("");
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

		if(!_server_con.submit(prob_id, fd, lang))
			QMessageBox::critical(this, "Error", "An error has occured whilst submitting your solution!", "O&k");
		::close(fd);
	}
}
	
void MainWindow::customEvent(QCustomEvent *ev) {
	LogMessage* msg = (LogMessage*)ev->data();
	switch(msg->prio_level) {
	case LOG_DEBUG:
	case LOG_NOTICE:
		break;
	case LOG_INFO:
		QMessageBox::information(this, "Info Message", msg->msg, "O&k");
		break;
	case LOG_WARNING:
		QMessageBox::warning(this, "Warning", msg->msg, "O&k");
		break;
	case LOG_ERR:
	case LOG_CRIT:
		QMessageBox::critical(this, "Error", msg->msg, "O&k");
		break;
	};
	delete msg;
}
