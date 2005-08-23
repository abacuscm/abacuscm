cc=g++
cflags=-g -ggdb -O0 -Iinclude -W -Wall -fpic
dflags=-Iinclude
ldflags=-rdynamic -g -ggdb -O0 -Llib
name=netsniff

.PHONY: default
default : all

libabacus_name = lib/libabacus.so
libabacus_objects = config \
	logger \
	moduleloader \
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
	messageblock
$(libabacus_name) : ldflags += -shared -ldl -lssl

abacusd_name = bin/abacusd
abacusd_objects = abacusd \
	sigsegv
$(abacusd_name) : ldflags += -labacus -lpthread
$(abacusd_name) : $(libabacus_name)

abacus_name = bin/abacus
abacus_objects = abacus \
	ui_mainwindow moc_mainwindow

$(abacus_name) : ldflags += -L$(QTDIR)/lib -lqt

modules = udpmessenger \
	dbmysql \
	act_passwd \
	act_adduser \
	act_addserver \
	act_auth
modules_d = $(foreach mod,$(modules),modules/mod_$(mod).so)
$(modules_d) : ldflags += -shared -labacus

modules/mod_dbmysql.so : ldflags += -lmysqlclient

###############################################################
depfiles=$(foreach m,$(libabacus_objects) $(abacusd_objects) $(abacus_objects) $(modules),deps/$(m).d)
abacusd_objects_d = $(foreach m,$(abacusd_objects),obj/$(m).o)
abacus_objects_d = $(foreach m,$(abacus_objects),obj/$(m).o)
libabacus_objects_d = $(foreach m,$(libabacus_objects),obj/$(m).o)

$(foreach m,$(abacus_objects),deps/$(m).d) : dflags += -I$(QTDIR)/include
$(foreach m,$(abacus_objects),obj/$(m).o) : cflags += -I$(QTDIR)/include

.PHONY: all
all : $(libabacus_name) $(abacusd_name) $(abacus_name) $(modules_d)

$(abacusd_name) : $(abacusd_objects_d)
	@[ -d bin ] || mkdir bin
	$(cc) $(ldflags) -o $@ $(abacusd_objects_d)

$(abacus_name) : $(abacus_objects_d)
	@[ -d bin ] || mkdir bin
	$(cc) $(ldflags) -o $@ $(abacus_objects_d)

$(libabacus_name) : $(libabacus_objects_d)
	@[ -d lib ] || mkdir lib
	$(cc) $(ldflags) -o $@ $(libabacus_objects_d)

modules/mod_%.so : obj/%.o
	@[ -d modules ] || mkdir modules
	$(cc) $(ldflags) -o $@ $<

obj/%.o : src/%.C Makefile
	@[ -d obj ] || mkdir obj
	$(cc) $(cflags) -o $@ -c $<

obj/%.o : src/%.c Makefile
	@[ -d obj ] || mkdir obj
	$(cc) -x c $(cflags) -o $@ -c $<

include/ui_%.h : ui/%.ui
	uic $< -o $@

src/moc_%.C : include/ui_%.h
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

.PHONY: distclean
distclean : clean
	rm -rf bin
	rm -rf lib
	rm -rf modules

deps/%.d : src/%.C Makefile
	@[ -d deps ] || mkdir deps
	$(cc) $(dflags) -MM $< | sed -e 's:$*.o:obj/$*.o $@:' > $@ || rm $@

# need some protection here, must include iff the not all of targets in (clean,distclean).
-include $(depfiles)
