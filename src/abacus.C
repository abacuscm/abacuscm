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

class LogEvent : public GUIEvent {
private:
	int _level;
	char *_msg;
public:
	LogEvent(int level, char* msg);
	virtual ~LogEvent();

	virtual void process(QWidget*);
};

LogEvent::LogEvent(int level, char* msg) {
	_level = level;
	_msg = msg;
}

LogEvent::~LogEvent() {
	free(_msg);
}

void LogEvent::process(QWidget *p) {
	switch(_level) {
	case LOG_DEBUG:
	case LOG_NOTICE:
		break;
	case LOG_INFO:
		QMessageBox::information(p, "Info Message", _msg, "O&k");
		break;
	case LOG_WARNING:
		QMessageBox::warning(p, "Warning", _msg, "O&k");
		break;
	case LOG_ERR:
	case LOG_CRIT:
		QMessageBox::critical(p, "Error", _msg, "O&k");
		break;
	}
}

static void window_log(int priority, const char* format, va_list ap) {
	char *msg;
	vasprintf(&msg, format, ap);

	LogEvent *le = new LogEvent(priority, msg);
	le->post();
}

int main(int argc, char** argv) {
	setup_sigsegv();

	Config &config = Config::getConfig();

	config.load("/etc/abacus.conf");
	char *home = getenv("HOME");
	if(home)
		config.load(string(home) + "/.abacus");
	config.load("abacus.conf");

	ServerConnection servercon;

	QApplication application(argc, argv);
	MainWindow mainwindow;
	application.setMainWidget(&mainwindow);
	mainwindow.show();

	GUIEvent::setReceiver(&mainwindow);
	log(LOG_INFO, "abacus is ready to fire up!");
	register_log_listener(window_log);

	return application.exec();
}
