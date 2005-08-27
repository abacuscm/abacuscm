#include "mainwindow.h"

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
	application.setMainWidget(&mainwindow);
	mainwindow.show();

	return application.exec();
}
