# Abacus Makefile (Copyright 2005 - 2006 Kroon Information Systems)
# $Id$

libdir=$(CURDIR)/lib
cc=g++
uic=$(QTDIR)/bin/uic
moc=$(QTDIR)/bin/moc
cflags=-g -ggdb -O0 -Iinclude -W -Wall -fpic
dflags=-Iinclude
ldflags=-rdynamic -g -ggdb -O0 -Llib -Wl,-rpath,$(libdir)

.PHONY: default
default: all

libabacus_name = lib/libabacus.so
libabacus_objects = config \
	logger \
	messageblock
$(libabacus_name) : ldflags += -shared -lssl

libabacus_s_name = lib/libabacus-server.so
libabacus_s_objects = moduleloader \
	supportmodule \
	peermessenger \
	message \
	dbcon \
	message_createserver \
	message_createuser \
	server \
	socket \
	clientlistener \
	clientconnection \
	clientaction \
	eventregister \
	timedaction \
	markers \
	problemtype
$(libabacus_s_name) : ldflags += -shared -ldl -labacus

libabacus_c_name = lib/libabacus-client.so
libabacus_c_objects = serverconnection
$(libabacus_c_name) : ldflags += -shared -labacus -pthread

abacusd_name = bin/abacusd
abacusd_objects = abacusd \
	sigsegv
$(abacusd_name) : ldflags += -labacus-server -lpthread
$(abacusd_name) : $(libabacus_s_name)
$(abacusd_name) : $(libabacus_name)

abacus_name = bin/abacus
abacus_objects = abacus \
	sigsegv \
	mainwindow \
	problemconfig \
	misc \
	guievent \
	ui_logindialog moc_ui_logindialog \
	ui_aboutdialog moc_ui_aboutdialog \
	ui_adduser moc_ui_adduser \
	ui_submit moc_ui_submit \
	ui_problemconfigbase moc_ui_problemconfigbase \
	ui_mainwindowbase moc_ui_mainwindowbase \
	ui_changepassworddialog moc_ui_changepassworddialog \
	ui_judgedecisiondialog moc_ui_judgedecisiondialog \
	ui_clarificationrequest moc_ui_clarificationrequest \
	ui_viewclarificationrequest moc_ui_viewclarificationrequest \
	viewclarificationrequestsub \
	ui_clarificationreply moc_ui_clarificationreply \
	ui_viewclarificationreply moc_ui_viewclarificationreply \
	ui_problemsubscription moc_ui_problemsubscription \
	ui_startstopdialog moc_ui_startstopdialog \
	ui_compileroutputdialog moc_ui_compileroutputdialog
$(abacus_name) : ldflags += -L$(QTDIR)/lib -lqt -labacus-client
$(abacus_name) : $(libabacus_c_name)
$(abacus_name) : $(libabacus_name)

balloon_name = bin/balloon
balloon_objects = balloon \
	sigsegv
$(balloon_name) : ldflags += -labacus-client

createuser_name = bin/createuser
createuser_objects = createuser \
	sigsegv
$(createuser_name) : ldflags += -labacus-client

runlimit_name = bin/runlimit
runlimit_objects = runlimit
$(runlimit_name) : cc=gcc

markerd_name = bin/markerd
markerd_objects = markerd \
	problemmarker \
	compiledproblemmarker \
	testcaseproblemmarker \
	runinfo \
	misc \
	userprog \
	c_cpp_userprog \
	java_userprog \
	buffer \
	serverconnection \
	sigsegv
$(markerd_name) : ldflags += -labacus -lpthread

modules = udpmessenger \
	dbmysql \
	act_addserver \
	act_adduser \
	act_auth \
	act_clarify \
	act_contesttime \
	act_events \
	act_mark \
	act_passwd \
	act_problem_subscription \
	act_problemmanip \
	act_problemtypes \
	act_serverlist \
	act_standings \
	act_startstop \
	act_status \
	act_submit \
	act_whatami \
	prob_testcasedriventype \
	support_standings \
	support_submission \
	support_timer


modules_d = $(foreach mod,$(modules),modules/mod_$(mod).so)
$(modules_d) : ldflags += -shared -labacus-server

modules/mod_dbmysql.so : ldflags += -lmysqlclient
obj/sigsegv.o : cflags += -DCPP_DEMANGLE

###############################################################
depfiles=$(foreach m,$(libabacus_objects) $(libabacus_s_objects) $(libabacus_c_objects) $(abacusd_objects) $(abacus_objects) $(modules) $(balloon_objects) $(createuser_objects) $(runlimit_objects) $(markerd_objects),deps/$(m).d)
abacusd_objects_d = $(foreach m,$(abacusd_objects),obj/$(m).o)
abacus_objects_d = $(foreach m,$(abacus_objects),obj/$(m).o)
libabacus_objects_d = $(foreach m,$(libabacus_objects),obj/$(m).o)
libabacus_c_objects_d = $(foreach m,$(libabacus_c_objects),obj/$(m).o)
libabacus_s_objects_d = $(foreach m,$(libabacus_s_objects),obj/$(m).o)
runlimit_objects_d = $(foreach m,$(runlimit_objects),obj/$(m).o)
markerd_objects_d = $(foreach m,$(markerd_objects),obj/$(m).o)
balloon_objects_d = $(foreach m,$(balloon_objects),obj/$(m).o)
createuser_objects_d = $(foreach m,$(createuser_objects),obj/$(m).o)

$(foreach m,$(abacus_objects),deps/$(m).d) : dflags += -I$(QTDIR)/include
$(foreach m,$(abacus_objects),obj/$(m).o) : cflags += -I$(QTDIR)/include

.DELETE_ON_ERROR:

.PHONY: all
all : client server modules marker
#$(libabacus_name) $(libabacus_s_name) $(libabacus_c_name) $(abacusd_name) $(abacus_name) $(modules_d)

.PHONY: client
client: $(libabacus_name) $(libabacus_c_name) $(abacus_name) $(balloon_name) $(createuser_name)

.PHONY: server
server: $(libabacus_name) $(libabacus_s_name) $(abacusd_name)

.PHONY: marker
marker: $(runlimit_name) $(markerd_name)

.PHONY: modules
modules: $(modules_d)

$(abacusd_name) : $(abacusd_objects_d) | $(dir $(abacusd_name)).d
	$(cc) $(ldflags) -o $@ $(abacusd_objects_d)

$(abacus_name) : $(abacus_objects_d) | $(dir $(abacus_name)).d
	$(cc) $(ldflags) -o $@ $(abacus_objects_d)

$(libabacus_name) : $(libabacus_objects_d) | $(dir $(libabacus_name)).d
	$(cc) $(ldflags) -o $@ $(libabacus_objects_d)

$(libabacus_c_name) : $(libabacus_c_objects_d) | $(dir $(libabacus_c_name)).d
	$(cc) $(ldflags) -o $@ $(libabacus_c_objects_d)

$(libabacus_s_name) : $(libabacus_s_objects_d) | $(dir $(libabacus_s_name)).d
	$(cc) $(ldflags) -o $@ $(libabacus_s_objects_d)

$(runlimit_name) : $(runlimit_objects_d) | $(dir $(runlimit_name)).d
	$(cc) $(ldflags) -o $@ $(runlimit_objects_d)

$(markerd_name) : $(markerd_objects_d) | $(dir $(markerd_name)).d
	$(cc) $(ldflags) -o $@ $(markerd_objects_d)

$(balloon_name) : $(balloon_objects_d) | $(dir $(balloon_name)).d
	$(cc) $(ldflags) -o $@ $(balloon_objects_d)

$(createuser_name) : $(createuser_objects_d) | $(dir $(createuser_name)).d
	$(cc) $(ldflags) -o $@ $(createuser_objects_d)

modules/mod_%.so : obj/%.o | modules/.d
	$(cc) $(ldflags) -o $@ $<

obj/%.o : src/%.C Makefile | $(dir obj/%).d
	$(cc) $(cflags) -o $@ -c $<

obj/%.o : src/%.c Makefile | $(dir obj/%).d
	$(cc) -x c $(cflags) -o $@ -c $<

.PRECIOUS: include/ui_%.h
include/ui_%.h : ui/%.ui
	$(uic) $< -o $@

.PRECIOUS: src/moc_%.C
src/moc_%.C : include/%.h
	$(moc) $< -o $@

.PRECIOUS: src/ui_%.C
src/ui_%.C : ui/%.ui
	$(uic) -impl ui_$*.h $< -o $@

.PHONY: clean
clean :
	rm -rf deps
	rm -rf obj
	rm -f include/ui_*.h
	rm -f src/moc_*.C
	rm -f src/ui_*.C
	rm -f ui/*~
	find doc -maxdepth 1 -type f ! -iname "*.tex" ! -iname "*.pdf"  ! -iname "*.txt" -exec rm {} \;
	
.PHONY: distclean
distclean : clean
	rm -rf bin
	rm -rf lib
	rm -rf modules
	rm -f doc/*.pdf

deps/%.d : src/%.C | deps/.d
	$(cc) $(dflags) -MM $< | sed -e 's:$*.o:obj/$*.o $@:' > $@

deps/%.d : src/%.c | deps/.d
	$(cc) -x c $(dflags) -MM $< | sed -e 's:$*.o:obj/$*.o $@:' > $@

.PRECIOUS: %/.d
%/.d :
	mkdir -p $(dir $@)
	@touch $@

depinc := 1
ifneq (,$(filter clean distclean,$(MAKECMDGOALS)))
depinc := 0
endif
ifneq (,$(filter-out clean distclean,$(MAKECMDGOALS)))
depinc := 1
endif

ifeq ($(depinc),1)
-include $(depfiles)
endif
