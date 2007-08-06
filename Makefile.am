# Set up directories
SUBDIRS = libltdl doc
AM_CPPFLAGS = -I$(srcdir)/include -I./include $(LTDLINC) -DSYSCONFDIR=\"$(sysconfdir)\" -DPKGLIBDIR=\"$(pkglibdir)\"

# Figure out what to compile
bin_PROGRAMS =
lib_LTLIBRARIES =
pkglib_LTLIBRARIES =
EXTRA_DIST =

if DO_SERVER
bin_PROGRAMS += bin/abacusd
lib_LTLIBRARIES += lib/libabacus.la lib/libabacus-server.la
pkglib_LTLIBRARIES += modules/mod_act_addserver.la \
		      modules/mod_act_adduser.la \
		      modules/mod_act_auth.la \
		      modules/mod_act_clarify.la \
		      modules/mod_act_contesttime.la \
		      modules/mod_act_events.la \
		      modules/mod_act_mark.la \
		      modules/mod_act_passwd.la \
		      modules/mod_act_problem_subscription.la \
		      modules/mod_act_problemmanip.la \
		      modules/mod_act_problemtypes.la \
		      modules/mod_act_serverlist.la \
		      modules/mod_act_standings.la \
		      modules/mod_act_startstop.la \
		      modules/mod_act_status.la \
		      modules/mod_act_submit.la \
		      modules/mod_act_whatami.la \
		      modules/mod_dbmysql.la \
		      modules/mod_prob_testcasedriventype.la \
		      modules/mod_support_standings.la \
		      modules/mod_support_submission.la \
		      modules/mod_support_timer.la \
		      modules/mod_udpmessenger.la
endif
if DO_MARKER
bin_PROGRAMS += bin/markerd bin/runlimit
lib_LTLIBRARIES += lib/libabacus.la
endif
if DO_CLIENT
bin_PROGRAMS += bin/abacus bin/balloon bin/createuser bin/standings
lib_LTLIBRARIES += lib/libabacus.la lib/libabacus-client.la
endif

# Programs
bin_markerd_SOURCES = src/markerd.C \
		      src/problemmarker.C include/problemmarker.h\
		      src/compiledproblemmarker.C include/compiledproblemmarker.h \
		      src/testcaseproblemmarker.C include/testcaseproblemmarker.h \
		      src/runinfo.C include/runinfo.h \
		      src/misc.C include/misc.h \
		      src/userprog.C include/userprog.h \
		      src/c_cpp_userprog.C \
		      src/java_userprog.C \
		      src/buffer.C include/buffer.h \
		      src/sigsegv.c include/sigsegv.h
bin_markerd_LDADD = lib/libabacus.la lib/libabacus-client.la

bin_runlimit_SOURCES = src/runlimit.c

bin_abacus_SOURCES = src/abacus.C \
		     src/sigsegv.c include/sigsegv.h \
		     src/mainwindow.C include/mainwindow.h \
		     src/problemconfig.C include/problemconfig.h \
		     src/guievent.C include/guievent.h \
		     src/viewclarificationrequestsub.C include/viewclarificationrequestsub.h
nodist_bin_abacus_SOURCES = $(gui_sources) $(gui_headers)
bin_abacus_LDADD = lib/libabacus-client.la $(QT_LIBS)
bin_abacus_CXXFLAGS = $(QT_CFLAGS)

bin_balloon_SOURCES = src/balloon.C src/sigsegv.c include/sigsegv.h
bin_balloon_LDADD = lib/libabacus-client.la
bin_standings_SOURCES = src/standings.C src/sigsegv.c include/sigsegv.h
bin_standings_LDADD = lib/libabacus-client.la
bin_createuser_SOURCES = src/createuser.C src/sigsegv.c include/sigsegv.h
bin_createuser_LDADD = lib/libabacus-client.la

bin_abacusd_SOURCES = src/abacusd.C \
		      src/sigsegv.c include/sigsegv.h
bin_abacusd_LDADD = lib/libabacus-server.la $(LIBLTDL)

# Support libraries
lib_libabacus_la_SOURCES = src/acmconfig.C include/acmconfig.h \
			   src/logger.C include/logger.h \
			   src/messageblock.C include/messageblock.h

lib_libabacus_server_la_SOURCES = src/moduleloader.C include/moduleloader.h \
				  src/supportmodule.C include/supportmodule.h \
				  src/peermessenger.C include/peermessenger.h \
				  src/message.C include/message.h \
				  src/dbcon.C include/dbcon.h \
				  src/message_createserver.C include/message_createserver.h \
				  src/message_createuser.C include/message_createuser.h \
				  include/message_type_ids.h \
				  src/server.C include/server.h include/queue.h \
				  src/socket.C include/socket.h \
				  src/clientlistener.C include/clientlistener.h \
				  src/clientconnection.C include/clientconnection.h \
				  src/clientaction.C include/clientaction.h \
				  src/eventregister.C include/eventregister.h \
				  src/timedaction.C include/timedaction.h \
				  src/markers.C include/markers.h \
				  src/problemtype.C include/problemtype.h
lib_libabacus_server_la_LIBADD = lib/libabacus.la

lib_libabacus_client_la_SOURCES = src/serverconnection.C include/serverconnection.h
lib_libabacus_client_la_LIBADD = lib/libabacus.la

# Modules
modules_mod_act_addserver_la_SOURCES = src/act_addserver.C
modules_mod_act_addserver_la_LIBADD = lib/libabacus-server.la
modules_mod_act_addserver_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_adduser_la_SOURCES = src/act_adduser.C
modules_mod_act_adduser_la_LIBADD = lib/libabacus-server.la
modules_mod_act_adduser_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_auth_la_SOURCES = src/act_auth.C
modules_mod_act_auth_la_LIBADD = lib/libabacus-server.la
modules_mod_act_auth_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_clarify_la_SOURCES = src/act_clarify.C
modules_mod_act_clarify_la_LIBADD = lib/libabacus-server.la
modules_mod_act_clarify_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_contesttime_la_SOURCES = src/act_contesttime.C include/timersupportmodule.h
modules_mod_act_contesttime_la_LIBADD = lib/libabacus-server.la
modules_mod_act_contesttime_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_events_la_SOURCES = src/act_events.C
modules_mod_act_events_la_LIBADD = lib/libabacus-server.la
modules_mod_act_events_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_mark_la_SOURCES = src/act_mark.C \
				  include/standingssupportmodule.h
modules_mod_act_mark_la_LIBADD = lib/libabacus-server.la
modules_mod_act_mark_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_passwd_la_SOURCES = src/act_passwd.C
modules_mod_act_passwd_la_LIBADD = lib/libabacus-server.la
modules_mod_act_passwd_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_problem_subscription_la_SOURCES = src/act_problem_subscription.C
modules_mod_act_problem_subscription_la_LIBADD = lib/libabacus-server.la
modules_mod_act_problem_subscription_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_problemmanip_la_SOURCES = src/act_problemmanip.C
modules_mod_act_problemmanip_la_LIBADD = lib/libabacus-server.la
modules_mod_act_problemmanip_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_problemtypes_la_SOURCES = src/act_problemtypes.C
modules_mod_act_problemtypes_la_LIBADD = lib/libabacus-server.la
modules_mod_act_problemtypes_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_serverlist_la_SOURCES = src/act_serverlist.C
modules_mod_act_serverlist_la_LIBADD = lib/libabacus-server.la
modules_mod_act_serverlist_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_standings_la_SOURCES = src/act_standings.C \
				       include/standingssupportmodule.h
modules_mod_act_standings_la_LIBADD = lib/libabacus-server.la
modules_mod_act_standings_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_startstop_la_SOURCES = src/act_startstop.C \
				       include/timersupportmodule.h
modules_mod_act_startstop_la_LIBADD = lib/libabacus-server.la
modules_mod_act_startstop_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_status_la_SOURCES = src/act_status.C \
				    include/timersupportmodule.h
modules_mod_act_status_la_LIBADD = lib/libabacus-server.la
modules_mod_act_status_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_submit_la_SOURCES = src/act_submit.C \
				    include/timersupportmodule.h \
				    include/submissionsupportmodule.h
modules_mod_act_submit_la_LIBADD = lib/libabacus-server.la
modules_mod_act_submit_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_act_whatami_la_SOURCES = src/act_whatami.C
modules_mod_act_whatami_la_LIBADD = lib/libabacus-server.la
modules_mod_act_whatami_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_dbmysql_la_SOURCES = src/dbmysql.C
modules_mod_dbmysql_la_LIBADD = lib/libabacus-server.la -lmysqlclient_r
modules_mod_dbmysql_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_prob_testcasedriventype_la_SOURCES = src/prob_testcasedriventype.C \
						 include/compiledproblemtype.h
modules_mod_prob_testcasedriventype_la_LIBADD = lib/libabacus-server.la
modules_mod_prob_testcasedriventype_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_support_standings_la_SOURCES = src/support_standings.C \
					   include/timersupportmodule.h \
					   include/usersupportmodule.h \
					   include/standingssupportmodule.h \
					   include/scoped_lock.h
modules_mod_support_standings_la_LIBADD = lib/libabacus-server.la
modules_mod_support_standings_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_support_submission_la_SOURCES = src/support_submission.C \
					    include/submissionsupportmodule.h
modules_mod_support_submission_la_LIBADD = lib/libabacus-server.la
modules_mod_support_submission_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_support_timer_la_SOURCES = src/support_timer.C include/timersupportmodule.h
modules_mod_support_timer_la_LIBADD = lib/libabacus-server.la
modules_mod_support_timer_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

modules_mod_udpmessenger_la_SOURCES = src/udpmessenger.C \
				      include/scoped_lock.h
modules_mod_udpmessenger_la_LIBADD = lib/libabacus-server.la
modules_mod_udpmessenger_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

# Other
libtool: @LIBTOOL_DEPS@
	$(SHELL) ./config.status --recheck

doc/usermanual/usermanual.pdf: doc/usermanual/usermanual.tex
	$(mkdir_p) doc/usermanual
	pdflatex $(srcdir)/doc/usermanual/usermanual.tex

# automake ought to take care of the .libs files
CLEANFILES = $(gui_sources) $(gui_headers) \
	     bin/.libs/abacus \
	     bin/.libs/abacusd \
	     bin/.libs/balloon \
	     bin/.libs/createuser \
	     bin/.libs/markerd \
	     bin/.libs/standings

# QT code; to regenerate this, run 'ls ui/*.ui | misc/qtdeps.h
# Generated by misc/qtdeps.pl at Wed Nov 22 13:42:30 2006
if DO_CLIENT
gui_sources =
gui_headers =
include/ui_aboutdialog.h: $(srcdir)/ui/aboutdialog.ui
	$(mkdir_p) include
	$(UIC) $(srcdir)/ui/aboutdialog.ui -o include/ui_aboutdialog.h
src/moc_ui_aboutdialog.C: include/ui_aboutdialog.h
	$(MOC) include/ui_aboutdialog.h -o src/moc_ui_aboutdialog.C
src/ui_aboutdialog.C: $(srcdir)/ui/aboutdialog.ui
	$(UIC) -impl ui_aboutdialog.h $(srcdir)/ui/aboutdialog.ui -o src/ui_aboutdialog.C
gui_sources += src/moc_ui_aboutdialog.C src/ui_aboutdialog.C
gui_headers += include/ui_aboutdialog.h
EXTRA_DIST += ui/aboutdialog.ui

include/ui_adduser.h: $(srcdir)/ui/adduser.ui
	$(mkdir_p) include
	$(UIC) $(srcdir)/ui/adduser.ui -o include/ui_adduser.h
src/moc_ui_adduser.C: include/ui_adduser.h
	$(MOC) include/ui_adduser.h -o src/moc_ui_adduser.C
src/ui_adduser.C: $(srcdir)/ui/adduser.ui
	$(UIC) -impl ui_adduser.h $(srcdir)/ui/adduser.ui -o src/ui_adduser.C
gui_sources += src/moc_ui_adduser.C src/ui_adduser.C
gui_headers += include/ui_adduser.h
EXTRA_DIST += ui/adduser.ui

include/ui_changepassworddialog.h: $(srcdir)/ui/changepassworddialog.ui
	$(mkdir_p) include
	$(UIC) $(srcdir)/ui/changepassworddialog.ui -o include/ui_changepassworddialog.h
src/moc_ui_changepassworddialog.C: include/ui_changepassworddialog.h
	$(MOC) include/ui_changepassworddialog.h -o src/moc_ui_changepassworddialog.C
src/ui_changepassworddialog.C: $(srcdir)/ui/changepassworddialog.ui
	$(UIC) -impl ui_changepassworddialog.h $(srcdir)/ui/changepassworddialog.ui -o src/ui_changepassworddialog.C
gui_sources += src/moc_ui_changepassworddialog.C src/ui_changepassworddialog.C
gui_headers += include/ui_changepassworddialog.h
EXTRA_DIST += ui/changepassworddialog.ui

include/ui_clarificationreply.h: $(srcdir)/ui/clarificationreply.ui
	$(mkdir_p) include
	$(UIC) $(srcdir)/ui/clarificationreply.ui -o include/ui_clarificationreply.h
src/moc_ui_clarificationreply.C: include/ui_clarificationreply.h
	$(MOC) include/ui_clarificationreply.h -o src/moc_ui_clarificationreply.C
src/ui_clarificationreply.C: $(srcdir)/ui/clarificationreply.ui
	$(UIC) -impl ui_clarificationreply.h $(srcdir)/ui/clarificationreply.ui -o src/ui_clarificationreply.C
gui_sources += src/moc_ui_clarificationreply.C src/ui_clarificationreply.C
gui_headers += include/ui_clarificationreply.h
EXTRA_DIST += ui/clarificationreply.ui

include/ui_clarificationrequest.h: $(srcdir)/ui/clarificationrequest.ui
	$(mkdir_p) include
	$(UIC) $(srcdir)/ui/clarificationrequest.ui -o include/ui_clarificationrequest.h
src/moc_ui_clarificationrequest.C: include/ui_clarificationrequest.h
	$(MOC) include/ui_clarificationrequest.h -o src/moc_ui_clarificationrequest.C
src/ui_clarificationrequest.C: $(srcdir)/ui/clarificationrequest.ui
	$(UIC) -impl ui_clarificationrequest.h $(srcdir)/ui/clarificationrequest.ui -o src/ui_clarificationrequest.C
gui_sources += src/moc_ui_clarificationrequest.C src/ui_clarificationrequest.C
gui_headers += include/ui_clarificationrequest.h
EXTRA_DIST += ui/clarificationrequest.ui

include/ui_compileroutputdialog.h: $(srcdir)/ui/compileroutputdialog.ui
	$(mkdir_p) include
	$(UIC) $(srcdir)/ui/compileroutputdialog.ui -o include/ui_compileroutputdialog.h
src/moc_ui_compileroutputdialog.C: include/ui_compileroutputdialog.h
	$(MOC) include/ui_compileroutputdialog.h -o src/moc_ui_compileroutputdialog.C
src/ui_compileroutputdialog.C: $(srcdir)/ui/compileroutputdialog.ui
	$(UIC) -impl ui_compileroutputdialog.h $(srcdir)/ui/compileroutputdialog.ui -o src/ui_compileroutputdialog.C
gui_sources += src/moc_ui_compileroutputdialog.C src/ui_compileroutputdialog.C
gui_headers += include/ui_compileroutputdialog.h
EXTRA_DIST += ui/compileroutputdialog.ui

include/ui_judgedecisiondialog.h: $(srcdir)/ui/judgedecisiondialog.ui
	$(mkdir_p) include
	$(UIC) $(srcdir)/ui/judgedecisiondialog.ui -o include/ui_judgedecisiondialog.h
src/moc_ui_judgedecisiondialog.C: include/ui_judgedecisiondialog.h
	$(MOC) include/ui_judgedecisiondialog.h -o src/moc_ui_judgedecisiondialog.C
src/ui_judgedecisiondialog.C: $(srcdir)/ui/judgedecisiondialog.ui
	$(UIC) -impl ui_judgedecisiondialog.h $(srcdir)/ui/judgedecisiondialog.ui -o src/ui_judgedecisiondialog.C
gui_sources += src/moc_ui_judgedecisiondialog.C src/ui_judgedecisiondialog.C
gui_headers += include/ui_judgedecisiondialog.h
EXTRA_DIST += ui/judgedecisiondialog.ui

include/ui_logindialog.h: $(srcdir)/ui/logindialog.ui
	$(mkdir_p) include
	$(UIC) $(srcdir)/ui/logindialog.ui -o include/ui_logindialog.h
src/moc_ui_logindialog.C: include/ui_logindialog.h
	$(MOC) include/ui_logindialog.h -o src/moc_ui_logindialog.C
src/ui_logindialog.C: $(srcdir)/ui/logindialog.ui
	$(UIC) -impl ui_logindialog.h $(srcdir)/ui/logindialog.ui -o src/ui_logindialog.C
gui_sources += src/moc_ui_logindialog.C src/ui_logindialog.C
gui_headers += include/ui_logindialog.h
EXTRA_DIST += ui/logindialog.ui

include/ui_mainwindowbase.h: $(srcdir)/ui/mainwindowbase.ui
	$(mkdir_p) include
	$(UIC) $(srcdir)/ui/mainwindowbase.ui -o include/ui_mainwindowbase.h
src/moc_ui_mainwindowbase.C: include/ui_mainwindowbase.h
	$(MOC) include/ui_mainwindowbase.h -o src/moc_ui_mainwindowbase.C
src/ui_mainwindowbase.C: $(srcdir)/ui/mainwindowbase.ui
	$(UIC) -impl ui_mainwindowbase.h $(srcdir)/ui/mainwindowbase.ui -o src/ui_mainwindowbase.C
gui_sources += src/moc_ui_mainwindowbase.C src/ui_mainwindowbase.C
gui_headers += include/ui_mainwindowbase.h
EXTRA_DIST += ui/mainwindowbase.ui

include/ui_problemconfigbase.h: $(srcdir)/ui/problemconfigbase.ui
	$(mkdir_p) include
	$(UIC) $(srcdir)/ui/problemconfigbase.ui -o include/ui_problemconfigbase.h
src/moc_ui_problemconfigbase.C: include/ui_problemconfigbase.h
	$(MOC) include/ui_problemconfigbase.h -o src/moc_ui_problemconfigbase.C
src/ui_problemconfigbase.C: $(srcdir)/ui/problemconfigbase.ui
	$(UIC) -impl ui_problemconfigbase.h $(srcdir)/ui/problemconfigbase.ui -o src/ui_problemconfigbase.C
gui_sources += src/moc_ui_problemconfigbase.C src/ui_problemconfigbase.C
gui_headers += include/ui_problemconfigbase.h
EXTRA_DIST += ui/problemconfigbase.ui

include/ui_problemsubscription.h: $(srcdir)/ui/problemsubscription.ui
	$(mkdir_p) include
	$(UIC) $(srcdir)/ui/problemsubscription.ui -o include/ui_problemsubscription.h
src/moc_ui_problemsubscription.C: include/ui_problemsubscription.h
	$(MOC) include/ui_problemsubscription.h -o src/moc_ui_problemsubscription.C
src/ui_problemsubscription.C: $(srcdir)/ui/problemsubscription.ui
	$(UIC) -impl ui_problemsubscription.h $(srcdir)/ui/problemsubscription.ui -o src/ui_problemsubscription.C
gui_sources += src/moc_ui_problemsubscription.C src/ui_problemsubscription.C
gui_headers += include/ui_problemsubscription.h
EXTRA_DIST += ui/problemsubscription.ui

include/ui_startstopdialog.h: $(srcdir)/ui/startstopdialog.ui
	$(mkdir_p) include
	$(UIC) $(srcdir)/ui/startstopdialog.ui -o include/ui_startstopdialog.h
src/moc_ui_startstopdialog.C: include/ui_startstopdialog.h
	$(MOC) include/ui_startstopdialog.h -o src/moc_ui_startstopdialog.C
src/ui_startstopdialog.C: $(srcdir)/ui/startstopdialog.ui
	$(UIC) -impl ui_startstopdialog.h $(srcdir)/ui/startstopdialog.ui -o src/ui_startstopdialog.C
gui_sources += src/moc_ui_startstopdialog.C src/ui_startstopdialog.C
gui_headers += include/ui_startstopdialog.h
EXTRA_DIST += ui/startstopdialog.ui

include/ui_submit.h: $(srcdir)/ui/submit.ui
	$(mkdir_p) include
	$(UIC) $(srcdir)/ui/submit.ui -o include/ui_submit.h
src/moc_ui_submit.C: include/ui_submit.h
	$(MOC) include/ui_submit.h -o src/moc_ui_submit.C
src/ui_submit.C: $(srcdir)/ui/submit.ui
	$(UIC) -impl ui_submit.h $(srcdir)/ui/submit.ui -o src/ui_submit.C
gui_sources += src/moc_ui_submit.C src/ui_submit.C
gui_headers += include/ui_submit.h
EXTRA_DIST += ui/submit.ui

include/ui_viewclarificationreply.h: $(srcdir)/ui/viewclarificationreply.ui
	$(mkdir_p) include
	$(UIC) $(srcdir)/ui/viewclarificationreply.ui -o include/ui_viewclarificationreply.h
src/moc_ui_viewclarificationreply.C: include/ui_viewclarificationreply.h
	$(MOC) include/ui_viewclarificationreply.h -o src/moc_ui_viewclarificationreply.C
src/ui_viewclarificationreply.C: $(srcdir)/ui/viewclarificationreply.ui
	$(UIC) -impl ui_viewclarificationreply.h $(srcdir)/ui/viewclarificationreply.ui -o src/ui_viewclarificationreply.C
gui_sources += src/moc_ui_viewclarificationreply.C src/ui_viewclarificationreply.C
gui_headers += include/ui_viewclarificationreply.h
EXTRA_DIST += ui/viewclarificationreply.ui

include/ui_viewclarificationrequest.h: $(srcdir)/ui/viewclarificationrequest.ui
	$(mkdir_p) include
	$(UIC) $(srcdir)/ui/viewclarificationrequest.ui -o include/ui_viewclarificationrequest.h
src/moc_ui_viewclarificationrequest.C: include/ui_viewclarificationrequest.h
	$(MOC) include/ui_viewclarificationrequest.h -o src/moc_ui_viewclarificationrequest.C
src/ui_viewclarificationrequest.C: $(srcdir)/ui/viewclarificationrequest.ui
	$(UIC) -impl ui_viewclarificationrequest.h $(srcdir)/ui/viewclarificationrequest.ui -o src/ui_viewclarificationrequest.C
gui_sources += src/moc_ui_viewclarificationrequest.C src/ui_viewclarificationrequest.C
gui_headers += include/ui_viewclarificationrequest.h
EXTRA_DIST += ui/viewclarificationrequest.ui

BUILT_SOURCES = $(gui_sources) $(gui_headers)
endif
# End generated section
