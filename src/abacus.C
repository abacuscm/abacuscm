#include "ui_mainwindow.h"
#include "ui_logindialog.h"

#include "serverconnection.h"
#include "config.h"
#include "sigsegv.h"
#include "logger.h"

#include <qapplication.h>
#include <qlineedit.h>
#include <stdlib.h>
#include <string>
#include <openssl/ssl.h>

using namespace std;

int main(int argc, char** argv) {
	setup_sigsegv();

	Config &config = Config::getConfig();

	config.load("/etc/abacus.conf");
	config.load(string(getenv("HOME")) + "/.abacus");
	config.load("abacus.conf");

	ServerConnection servercon;

	QApplication application(argc, argv);
	MainWindow mainwindow;
	LoginDialog logindialog;

	logindialog.serverName->setText(config["server"]["address"]);
	logindialog.service->setText(config["server"]["service"]);

	if(!logindialog.exec())
		return 0;

	if(!servercon.connect(logindialog.serverName->text(),
				logindialog.service->text()))
		return -1;

	log(LOG_DEBUG, "Connected");

	int res;
	if(!servercon.auth(logindialog.username->text(),
				logindialog.password->text())) {
		log(LOG_ERR, "Authentication error");
		res = -1;
	} else {
		mainwindow.show();
		application.setMainWidget(&mainwindow);
		res = application.exec();
	}
	
	servercon.disconnect();

	return res;
}
