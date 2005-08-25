#include "ui_mainwindow.h"

#include "serverconnection.h"
#include "config.h"
#include "sigsegv.h"

#include <qapplication.h>
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

	if(!servercon.connect(config["server"]["address"], config["server"]["service"]))
		return -1;

	if(!servercon.auth("jaco", "jk857"))
		return -1;

	QApplication application(argc, argv);
	MainWindow mainwindow;
	
	mainwindow.show();
	application.setMainWidget(&mainwindow);

	return application.exec();
}
