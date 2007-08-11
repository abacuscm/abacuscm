# Set up directories
SUBDIRS = libltdl doc
AM_CPPFLAGS = -I$(srcdir)/include -I./include $(LTDLINC) -DSYSCONFDIR=\"$(sysconfdir)\" -DPKGLIBDIR=\"$(pkglibdir)\" -W -Wall

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
		      modules/mod_support_user.la \
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

modules_mod_support_user_la_SOURCES = src/support_user.C include/usersupportmodule.h
modules_mod_support_user_la_LIBADD = lib/libabacus-server.la
modules_mod_support_user_la_LDFLAGS = -thread-safe -module -export-dynamic -version 0:0:0

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

if DO_CLIENT
include $(top_srcdir)/Makefile.qt.am
endif
