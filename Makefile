-include Makefile.conf

mods ?= client server marker admintools
builddocs ?= no
TOPDIR ?= .

TARGET_BINS:=
TARGET_LIBS :=

CLIENT_MODS = serverconnection messageblock logger sigsegv acmconfig threadssl misc score permissions

ifneq ($(filter client,$(mods)),)
TARGET_BINS += abacus
MODS_abacus = abacus $(CLIENT_MODS) \
	guievent \
	moc_mainwindow mainwindow \
	problemconfig viewclarificationrequestsub

LIBS_abacus = ssl crypto pthread rt Qt3Support
NEED_QT3=1
bin/abacus : CFLAGS += $(QT_CFLAGS)
bin/abacus : LFLAGS += $(QT_LDFLAGS)
endif

ifneq ($(filter server,$(mods)),)
TARGET_BINS += abacusd
TARGET_LIBS += abacus
MODS_abacusd = abacusd
LIBS_abacusd = ssl crypto pthread abacus

LIBMODS_abacus = waitable socket \
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
	message_creategroup \
	clientconnection \
	clientaction \
	markers \
	clienteventregistry \
	problemtype \
	timedaction \
	threadssl \
	score \
	permissions \
	permissionmap \
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
	act_addgroup \
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
	permissionmap \
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
LIBS_markerd = ssl crypto pthread rt
endif

ifneq ($(filter admintools,$(mods)),)
TARGET_BINS += balloon adduser addgroup addproblem standings batch extract_source make_submission

MODS_balloon = balloon $(CLIENT_MODS)
LIBS_balloon = ssl crypto pthread rt

MODS_adduser = createuser $(CLIENT_MODS)
LIBS_adduser = ssl crypto pthread rt

MODS_addgroup = creategroup $(CLIENT_MODS)
LIBS_addgroup = ssl crypto pthread rt

MODS_addproblem = createproblem $(CLIENT_MODS)
LIBS_addproblem = ssl crypto pthread rt

MODS_batch = batch $(CLIENT_MODS)
LIBS_batch = ssl crypto pthread rt

MODS_standings = standings $(CLIENT_MODS)
LIBS_standings = ssl crypto pthread rt

MODS_extract_source = extract_source $(CLIENT_MODS)
LIBS_extract_source = ssl crypto pthread rt

MODS_make_submission = make_submission $(CLIENT_MODS)
LIBS_make_submission = ssl crypto pthread rt
endif

ifeq ($(builddocs),yes)
TARGET_DOCS=usermanual
endif
include $(TOPDIR)/Makefile.inc
