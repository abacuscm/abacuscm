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
#include <stdarg.h>

using namespace std;


static QWidget *mainWidget;

static void window_log(int priority, const char* format, va_list ap) {
	// Would have prefered to use QString::sprintf but it doesn't
	// support va_list arguments.  Can probably be fooled by using
	// assembler but then I lose compatibility with anything but x86.
	char *msg;
	vasprintf(&msg, format, ap);
	
	LogMessage *lmsg = new LogMessage;
	lmsg->msg = msg;
	
	free(msg);
	
	lmsg->prio_level = priority;
	
	QCustomEvent *ev = new QCustomEvent(QEvent::User);

	ev->setData(lmsg);

	QApplication::postEvent(mainWidget, ev);
}

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

	mainWidget = &mainwindow;
	register_log_listener(window_log);

	return application.exec();
}
