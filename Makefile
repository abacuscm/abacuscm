-include Makefile.conf

mods ?= server marker admintools docs
docs_profile ?= cxx;java;python;python3
TOPDIR ?= .

TARGET_BINS:=
TARGET_LIBS :=

CLIENT_MODS = serverconnection messageblock logger sigsegv acmconfig threadssl misc score permissions

ifneq ($(filter server,$(mods)),)
TARGET_BINS += abacusd
TARGET_LIBS += abacus
MODS_abacusd = abacusd
LIBS_abacusd = ssl crypto pthread abacus dl

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
	act_bonus \
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
LIBS_markerd = ssl crypto pthread rt dl
endif

ifneq ($(filter admintools,$(mods)),)
TARGET_BINS += abacustool batch

MODS_abacustool = abacustool $(CLIENT_MODS)
LIBS_abacustool = ssl crypto pthread rt dl

MODS_batch = batch $(CLIENT_MODS)
LIBS_batch = ssl crypto pthread rt dl
endif

ifneq ($(filter docs,$(mods)),)
TARGET_DOCS = abacuscm contestant

PDF_STYLESHEET_abacuscm = fo
HTML_STYLESHEET_abacuscm = xhtml
XML_abacuscm = abacuscm \
	contestant \
	docker administrator-core administrator-webapp \
	developer-core developer-webapp \
	licensing
XSLTPROC_ARGS_abacuscm = --stringparam profile.condition "$(docs_profile)"

PDF_STYLESHEET_contestant = fo
HTML_STYLESHEET_contestant = xhtml
XML_contestant = contestant
XSLTPROC_ARGS_contestant = --stringparam profile.condition "$(docs_profile)"
endif

include $(TOPDIR)/Makefile.inc
