-include Makefile.conf

mods ?= client server marker admintools
builddocs ?= no
TOPDIR ?= .

TARGET_BINS:=
TARGET_LIBS :=

ifneq ($(filter client,$(mods)),)
TARGET_BINS += abacus
MODS_abacus = abacus serverconnection messageblock logger \
	sigsegv acmconfig threadssl misc \
	guievent score \
	ui_mainwindowbase moc_ui_mainwindowbase mainwindow \
	ui_adduser moc_ui_adduser \
	ui_compileroutputdialog moc_ui_compileroutputdialog \
	ui_changepassworddialog moc_ui_changepassworddialog \
	ui_clarificationrequest moc_ui_clarificationrequest \
	ui_problemsubscription moc_ui_problemsubscription \
	ui_startstopdialog moc_ui_startstopdialog \
	ui_problemconfigbase moc_ui_problemconfigbase problemconfig \
	ui_aboutdialog moc_ui_aboutdialog \
	ui_judgedecisiondialogbase moc_ui_judgedecisiondialogbase \
	ui_submit moc_ui_submit \
	ui_logindialog moc_ui_logindialog \
	ui_viewclarificationrequest moc_ui_viewclarificationrequest viewclarificationrequestsub \
	ui_clarificationreply moc_ui_clarificationreply \
	ui_viewclarificationreply moc_ui_viewclarificationreply

LIBS_abacus = ssl crypto pthread
NEED_QT3=1
bin/abacus : LDFLAGS += $(QT_LDFLAGS)
bin/abacus : CFLAGS += $(QT_CFLAGS)
endif

ifneq ($(filter server,$(mods)),)
TARGET_BINS += abacusd
TARGET_LIBS += abacus
MODS_abacusd = abacusd
LIBS_abacusd = ssl crypto pthread abacus

LIBMODS_abacus = socket \
	clientlistener \
	dbcon \
	server \
	servereventregistry \
	peermessenger \
	moduleloader \
	logger \
	acmconfig \
	supportmodule \
	messageblock \
	message \
	message_createserver \
	message_createuser \
	clientconnection \
	clientaction \
	markers \
	clienteventregistry \
	problemtype \
	timedaction \
	threadssl \
	score \
	hashpw

TARGET_MODS += dbmysql \
	udtcpmessenger \
	udpmessenger \
	support_standings \
	support_submission \
	support_timer \
	support_user \
	support_keepalive \
	act_addserver \
	act_adduser \
	act_auth \
	act_clarify \
	act_contesttime \
	act_events \
	act_mark \
	act_passwd \
	act_problemmanip \
	act_problemtypes \
	act_serverlist \
	act_standings \
	act_startstop \
	act_status \
	act_submit \
	act_whatami \
	prob_testcasedriventype \
	prob_interactivetype

MODLIBS_dbmysql = mysqlclient_r
endif

ifneq ($(filter marker,$(mods)),)
TARGET_BINS += runlimit markerd
MODS_runlimit = runlimit
MODS_markerd = markerd \
	serverconnection \
	messageblock \
	acmconfig \
	problemmarker \
	buffer \
	runinfo \
	misc \
	logger \
	threadssl
# These are listed separately because the really aught to be modules
MODS_markerd += userprog \
	java_userprog \
	c_cpp_userprog \
	python_userprog \
	compiledproblemmarker \
	testcaseproblemmarker \
	interactiveproblemmarker \
	sigsegv
LIBS_markerd = ssl crypto pthread
endif

ifneq ($(filter admintools,$(mods)),)
TARGET_BINS += balloon adduser addproblem standings batch extract_source make_submission

MODS_balloon = balloon serverconnection messageblock logger sigsegv acmconfig threadssl
LIBS_balloon = ssl crypto pthread

MODS_adduser = createuser serverconnection messageblock logger sigsegv acmconfig threadssl
LIBS_adduser = ssl crypto pthread

MODS_addproblem = createproblem serverconnection messageblock logger sigsegv acmconfig threadssl
LIBS_addproblem = ssl crypto pthread

MODS_batch = batch serverconnection messageblock logger sigsegv acmconfig threadssl
LIBS_batch = ssl crypto pthread

MODS_standings = standings serverconnection messageblock logger sigsegv acmconfig threadssl score
LIBS_standings = ssl crypto pthread

MODS_extract_source = extract_source serverconnection messageblock logger sigsegv acmconfig threadssl
LIBS_extract_source = ssl crypto pthread

MODS_make_submission = make_submission serverconnection messageblock logger sigsegv acmconfig threadssl
LIBS_make_submission = ssl crypto pthread
endif

ifeq ($(builddocs),yes)
TARGET_DOCS=usermanual qna
endif
include $(TOPDIR)/Makefile.inc
