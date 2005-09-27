#ifndef __MAINWINDOW_H__
#define __MAINWINDOW_H__

#include "ui_mainwindowbase.h"
#include "ui_logindialog.h"

#include "serverconnection.h"

typedef struct {
	int prio_level;
	QString msg;
} LogMessage;

class MainWindow : public MainWindowBase {
private:
	LoginDialog _login_dialog;
	ServerConnection _server_con;

	std::string _active_type;
	
	void triggerType(std::string type, bool status);
	void switchType(std::string type);
protected:
	virtual void doHelpAbout();
	virtual void doFileConnect();
	virtual void doFileDisconnect();
	virtual void doAdminCreateUser();
	virtual void doAdminProblemConfig();
	virtual void doSubmit();

	virtual void customEvent(QCustomEvent *ev);
public:
	MainWindow();
	~MainWindow();
};

#endif
