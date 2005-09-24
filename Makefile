libdir=$(CURDIR)/lib
cc=g++
cflags=-g -ggdb -O0 -Iinclude -W -Wall -fpic
dflags=-Iinclude
ldflags=-rdynamic -g -ggdb -O0 -Llib -Wl,-rpath,$(libdir)

.PHONY: default
default : all

libabacus_name = lib/libabacus.so
libabacus_objects = config \
	logger \
	messageblock
$(libabacus_name) : ldflags += -shared

libabacus_s_name = lib/libabacus-server.so
libabacus_s_objects = moduleloader \
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
	problemtype
$(libabacus_s_name) : ldflags += -shared -ldl -lssl -labacus

libabacus_c_name = lib/libabacus-client.so
libabacus_c_objects = serverconnection
$(libabacus_c_name) : ldflags += -shared -lssl -labacus

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
	ui_logindialog moc_ui_logindialog \
	ui_aboutdialog moc_ui_aboutdialog \
	ui_adduser moc_ui_adduser \
	ui_problemconfigbase moc_ui_problemconfigbase \
	ui_mainwindowbase moc_ui_mainwindowbase
$(abacus_name) : ldflags += -L$(QTDIR)/lib -lqt -labacus-client
$(abacus_name) : $(libabacus_c_name)
$(abacus_name) : $(libabacus_name)

modules = udpmessenger \
	dbmysql \
	act_passwd \
	act_adduser \
	act_addserver \
	act_auth \
	act_whatami \
	act_problemtypes \
	act_problemmanip \
	act_status \
	prob_testcasedriventype
modules_d = $(foreach mod,$(modules),modules/mod_$(mod).so)
$(modules_d) : ldflags += -shared -labacus-server

modules/mod_dbmysql.so : ldflags += -lmysqlclient

###############################################################
depfiles=$(foreach m,$(libabacus_objects) $(libabacus_s_objects) $(libabacus_c_objects) $(abacusd_objects) $(abacus_objects) $(modules),deps/$(m).d)
abacusd_objects_d = $(foreach m,$(abacusd_objects),obj/$(m).o)
abacus_objects_d = $(foreach m,$(abacus_objects),obj/$(m).o)
libabacus_objects_d = $(foreach m,$(libabacus_objects),obj/$(m).o)
libabacus_c_objects_d = $(foreach m,$(libabacus_c_objects),obj/$(m).o)
libabacus_s_objects_d = $(foreach m,$(libabacus_s_objects),obj/$(m).o)

$(foreach m,$(abacus_objects),deps/$(m).d) : dflags += -I$(QTDIR)/include
$(foreach m,$(abacus_objects),obj/$(m).o) : cflags += -I$(QTDIR)/include

.PHONY: all
all : client server modules
#$(libabacus_name) $(libabacus_s_name) $(libabacus_c_name) $(abacusd_name) $(abacus_name) $(modules_d)

.PHONY: client
client: $(libabacus_name) $(libabacus_c_name) $(abacus_name)

.PHONY: server
server: $(libabacus_name) $(libabacus_s_name) $(abacusd_name)

.PHONY: modules
modules: $(modules_d)

$(abacusd_name) : $(abacusd_objects_d)
	@[ -d $(@D) ] || mkdir $(@D)
	$(cc) $(ldflags) -o $@ $(abacusd_objects_d)

$(abacus_name) : $(abacus_objects_d)
	@[ -d $(@D) ] || mkdir $(@D)
	$(cc) $(ldflags) -o $@ $(abacus_objects_d)

$(libabacus_name) : $(libabacus_objects_d)
	@[ -d $(@D) ] || mkdir $(@D)
	$(cc) $(ldflags) -o $@ $(libabacus_objects_d)

$(libabacus_c_name) : $(libabacus_c_objects_d)
	@[ -d $(@D) ] || mkdir $(@D)
	$(cc) $(ldflags) -o $@ $(libabacus_c_objects_d)

$(libabacus_s_name) : $(libabacus_s_objects_d)
	@[ -d $(@D) ] || mkdir $(@D)
	$(cc) $(ldflags) -o $@ $(libabacus_s_objects_d)

modules/mod_%.so : obj/%.o
	@[ -d $(@D) ] || mkdir $(@D)
	$(cc) $(ldflags) -o $@ $<

obj/%.o : src/%.C Makefile
	@[ -d $(@D) ] || mkdir $(@D)
	$(cc) $(cflags) -o $@ -c $<

obj/%.o : src/%.c Makefile
	@[ -d $(@D) ] || mkdir $(@D)
	$(cc) -x c $(cflags) -o $@ -c $<

include/ui_%.h : ui/%.ui
	uic $< -o $@

src/moc_%.C : include/%.h
	moc $< -o $@

src/ui_%.C : ui/%.ui
	uic -impl ui_$*.h $< -o $@

.PHONY: clean
clean :
	rm -rf deps
	rm -rf obj
	rm -f include/ui_*.h
	rm -f src/moc_*.C
	rm -f src/ui_*.C
	rm -f ui/*~
	find doc -maxdepth 1 -type f ! -iname "*.tex" ! -iname "*.pdf" -exec rm {} \;
	
.PHONY: distclean
distclean : clean
	rm -rf bin
	rm -rf lib
	rm -rf modules
	rm -f doc/*.pdf

deps/%.d : src/%.C
	@[ -d $(@D) ] || mkdir $(@D)
	$(cc) $(dflags) -MM $< | sed -e 's:$*.o:obj/$*.o $@:' > $@ || rm $@

deps/%.d : src/%.c
	@[ -d $(@D) ] || mkdir $(@D)
	$(cc) -x c $(dflags) -MM $< | sed -e 's:$*.o:obj/$*.o $@:' > $@ || rm $@

# need some protection here, must include iff the not all of targets in (clean,distclean).
-include $(depfiles)
