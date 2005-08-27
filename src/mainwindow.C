#include "mainwindow.h"
#include "ui_aboutdialog.h"

void MainWindow::doHelpAbout() {
	AboutDialog about;
	about.exec();
}
