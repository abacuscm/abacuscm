#include "ui_mainwindow.h"
#include "ui_logindialog.h"

#include "serverconnection.h"
#include "config.h"
#include "sigsegv.h"

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

	mainwindow.show();
	application.setMainWidget(&mainwindow);

	return application.exec();
}
