cc=g++
cflags=-g -ggdb -O2 -Iinclude -W -Wall -fpic
dflags=-Iinclude
ldflags=-rdynamic -g -ggdb -O2 -Llib
name=netsniff

.PHONY: default
default : all

libabacus_name = lib/libabacus.so
libabacus_objects = config \
	logger \
	moduleloader \
	peermessenger \
	messagebuilder \
	message \
	dbcon \
	message_createserver \
	server \
	socket \
	clientlistener
$(libabacus_name) : ldflags += -shared -ldl

abacusd_name = bin/abacusd
abacusd_objects = abacusd \
	sigsegv
$(abacusd_name) : ldflags += -labacus -lpthread
$(abacusd_name) : $(libabacus_name)

modules = udpmessenger dbmysql
modules_d = $(foreach mod,$(modules),modules/mod_$(mod).so)
$(modules_d) : ldflags += -shared -labacus

modules/mod_dbmysql.so : ldflags += -lmysqlclient

###############################################################
depfiles=$(foreach m,$(libabacus_objects) $(abacusd_objects) $(modules),deps/$(m).d)
abacusd_objects_d = $(foreach m,$(abacusd_objects),obj/$(m).o)
libabacus_objects_d = $(foreach m,$(libabacus_objects),obj/$(m).o)

.PHONY: all
all : $(libabacus_name) $(abacusd_name) $(modules_d)

$(abacusd_name) : $(abacusd_objects_d)
	@[ -d bin ] || mkdir bin
	$(cc) $(ldflags) -o $@ $(abacusd_objects_d)

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

.PHONY: clean
clean :
	rm -rf deps
	rm -rf obj

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
