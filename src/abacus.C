#include "mainwindow.h"

#include "serverconnection.h"
#include "config.h"
#include "sigsegv.h"
#include "logger.h"
#include "guievent.h"

#include <qapplication.h>
#include <qmessagebox.h>
#include <stdlib.h>
#include <string>
#include <openssl/ssl.h>
#include <stdarg.h>

using namespace std;

int main(int argc, char** argv) {
	setup_sigsegv();

	Config &config = Config::getConfig();

	config.load("/etc/abacus.conf");
	char *home = getenv("HOME");
	if(home)
		config.load(string(home) + "/.abacus");
	config.load("abacus.conf");

	log(LOG_INFO, "abacus config loaded, preparing to start abacus.");

	ServerConnection servercon;

	QApplication application(argc, argv);
	MainWindow mainwindow;
	application.setMainWidget(&mainwindow);
	mainwindow.show();

	return application.exec();
}
