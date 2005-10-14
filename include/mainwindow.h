#ifndef __MAINWINDOW_H__
#define __MAINWINDOW_H__

#include "ui_mainwindowbase.h"
#include "ui_logindialog.h"

#include "serverconnection.h"

class QCheckBox;
class QListViewItem;
class QTimer;
class QFileDialog;
class Submit;

typedef struct {
	int prio_level;
	QString msg;
} LogMessage;

class MainWindow : public MainWindowBase {
private:
	LoginDialog _login_dialog;
        Submit *_submit_dialog;
        QFileDialog *_submit_file_dialog;

	ServerConnection _server_con;

	std::string _active_type;

	/* Clock-related stuff */
	QTimer *timer;
	time_t projected_stop;

	void triggerType(std::string type, bool status);
	void switchType(std::string type);
protected:
	virtual void doHelpAbout();
	virtual void doFileConnect();
	virtual void doFileDisconnect();
	virtual void doAdminCreateUser();
	virtual void doAdminProblemConfig();
	virtual void doAdminStartStop();
	virtual void doSubmit();
	virtual void doClarificationRequest();
	virtual void doShowClarificationRequest(QListViewItem*);
        virtual void doShowClarificationReply(QListViewItem*);
        virtual void doTimer();
	virtual void tabChanged(QWidget*);
	virtual void doChangePassword();
	virtual void doJudgeSubscribeToProblems();
	virtual void submissionHandler(QListViewItem *);
        virtual void toggleBalloonPopups(bool);

	virtual void customEvent(QCustomEvent *ev);
public:
	MainWindow();
	~MainWindow();

	// made public so that my functors can get to them since
	// we can't cast member function pointers to void* - no idea 
	// why though - can probably memcpy them but that would be
	// even uglier.
	void updateStandings();
	virtual void updateSubmissions();
	void updateClarificationRequests();
	void updateClarifications();
        void updateStatus();
	void serverDisconnect();

	std::string getActiveType();
};

#endif
