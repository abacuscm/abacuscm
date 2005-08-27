#ifndef __MAINWINDOW_H__
#define __MAINWINDOW_H__

#include "ui_mainwindowbase.h"
#include "ui_logindialog.h"

#include "serverconnection.h"

class MainWindow : public MainWindowBase {
private:
	LoginDialog _login_dialog;
	ServerConnection _server_con;

protected:
	virtual void doHelpAbout();
	virtual void doFileConnect();
public:
	MainWindow();
	~MainWindow();
};

#endif
