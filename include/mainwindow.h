#ifndef __MAINWINDOW_H__
#define __MAINWINDOW_H__

#include "ui_mainwindowbase.h"
#include "ui_logindialog.h"

#include "serverconnection.h"

#include <map>
#include <list>

class MainWindow : public MainWindowBase {
private:
	typedef std::list<QAction*> ActionList;
	typedef std::map<std::string, ActionList> TypeActionMap;
	
	LoginDialog _login_dialog;
	ServerConnection _server_con;

	TypeActionMap _type_action_map;
	std::string _active_type;
	
	void activateType(std::string type);
protected:
	virtual void doHelpAbout();
	virtual void doFileConnect();
	virtual void doFileDisconnect();
	virtual void doAdminCreateUser();
	virtual void doAdminProblemConfig();

public:
	MainWindow();
	~MainWindow();
};

#endif
