#include "ui_mainwindow.h"

#include "serverconnection.h"

#include <qapplication.h>

int main(int argc, char** argv) {
	QApplication application(argc, argv);
	MainWindow mainwindow;
	ServerConnection servercon;

	mainwindow.show();
	application.setMainWidget(&mainwindow);

	return application.exec();
}
