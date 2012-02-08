# Should be included from other Makefiles.
# $Id: Makefile.inc 3771 2009-07-06 07:04:14Z jkroon $

# 0 == debug off, anything else switches DEBUG on
DEBUG ?= 1
STATIC ?= 0

# This is a C++ based lib.  End of story.
CC := g++
AR := ar
RANLIB ?= ranlib

CFLAGS ?= -W -Wall

CFLAGS += -Iinclude
LDFLAGS += -rdynamic -Llib -lrt -ldl

ifneq ($(EXTRA_FLAGS),)
CFLAGS += $(EXTRA_FLAGS)
LDFLAGS += $(EXTRA_FLAGS)
endif
ifneq ($(EXTRA_CFLAGS),)
CFLAGS += $(EXTRA_CFLAGS)
endif
ifneq ($(EXTRA_LDFLAGS),)
LDFLAGS += $(EXTRA_LDFLAGS)
endif

CFLAGS += $(OPTIM)
CFLAGS += $(DEFS)

LIBDIR := $(PWD)/lib

LATEX?=latex
DVIPS?=dvips
DVIPDF?=dvipdf

SVN?=svn

# Naming as per tex(1).
export TEXINPUTS=tex:$(TOPDIR)/tex:tex-tmp:
export DVIPSHEADERS=doc:$(TOPDIR)/tex:

ifneq ($(NEED_QT3),)
UIC:=$(shell for i in $$(which -a uic 2>&1); do $$i -version 2>&1 | awk '{ print $$NF }' | grep -q '^3\.' && echo $$i && break; done)
MOC:=$(shell for i in $$(which -a moc 2>&1); do $$i -v 2>&1 | awk '{ print $$NF }' | grep -q '^3\.' && echo $$i && break; done)
QT_CFLAGS:=$(shell pkg-config qt-mt --cflags)
QT_LDFLAGS:=$(shell  pkg-config qt-mt --libs)
endif

-include $(TOPDIR)/Makefile.extra

# Some checks for ensuring the applicable executables has been located.
ifneq ($(NEED_QT3),)
ifeq ($(UIC),)
$(error Error locating uic executable for Qt3 - please set UIC= in Makefile.extra)
endif
ifeq ($(MOC),)
$(error Error locating moc executable for Qt3 - please set MOC= in Makefile.extra)
endif
endif

##############################################################################################
#### From here on there should be no reason to change unless you know what you are doing. ####
##############################################################################################
.PHONY: default
default: all

LIB_TARGET_SO := $(foreach L,$(TARGET_LIBS),lib/lib$(L).so)
LIB_TARGET_A := $(foreach L,$(TARGET_LIBS),lib/lib$(L).a)
BIN_TARGET := $(foreach B,$(TARGET_BINS),bin/$(B))
MOD_TARGET := $(foreach M,$(TARGET_MODS),modules/$(M).so)
DOC_PDF := $(foreach D,$(TARGET_DOCS),doc/$(D).pdf)
DOC_PS := $(foreach D,$(TARGET_DOCS),doc/$(D).ps)

ALL_TARGET :=
ALL_TARGET += $(LIB_TARGET_SO)
ALL_TARGET += $(LIB_TARGET_A)
ALL_TARGET += $(BIN_TARGET)
ALL_TARGET += $(MOD_TARGET)
ALL_TARGET += $(DOC_PDF)

BINMODS := $(foreach B,$(TARGET_BINS),$(MODS_$(B)))
LIBMODS := $(foreach L,$(TARGET_LIBS),$(MODS_$(L)))
MODMODS := $(foreach M,$(TARGET_MODS),$M)
ALLMODS := $(BINMODS) $(LIBMODS) $(MODMODS)

$(LIBMODS:%=obj/%.o) $(MODMODS:%=obj/%.o): CFLAGS += -fpic

.PHONY: all
all: $(ALL_TARGET)
	@echo "Everything has been brought up to date"

.PHONY: docs pdfdoc psdoc
psdoc : $(DOC_PS)
pdfdoc : $(DOC_PDF)
docs : psdoc pdfdoc

# Delete targets if/when updating them fails.
.DELETE_ON_ERROR:

# some systems require -fpic on .so files.
ifeq ($(shell uname -m),x86_64)
$(patsubst %,obj/%.o,$(foreach L,$(TARGET_LIBS),$(LIBMODS_$(L)))) : CFLAGS += -fpic
endif

ifneq ($(DEBUG),0)
DEFS += -DDEBUG
CFLAGS += -g -ggdb
LDFLAGS += -g -ggdb
OPTIM = -O0
else
DEFS += -DNDEBUG
OPTIM = -O2
endif

ifneq ($(STATIC),0)
$(BIN_TARGET) : LDFLAGS += -static
endif

ifneq ($(OPTIM),)
CFLAGS += $(OPTIM)
endif

ifneq ($(LIBDIR),)
LDFLAGS += -Wl,-rpath,$(shell readlink -f "$(LIBDIR)")
endif

ifneq ($(VERBOSE),)
SHOWCL=
TEXMODE=nonstopmode
else
SHOWCL=@
TEXMODE=batchmode
endif

##########################################
# How to build actual binaries/libraries
#
$(LIB_TARGET_SO): | lib/.d
	@echo -e "\t[LD - SO] $@"
	$(SHOWCL)$(CC) -shared -o $@ $(foreach M,$(LIBMODS_$(patsubst lib/lib%.so,%,$@)),obj/$(M).o) $(LDFLAGS) $(foreach L,$(LIBS) $(LIBS_$(patsubst lib/lib%.so,%,$@)),-l$(L))


$(LIB_TARGET_A): | lib/.d
	@echo -e "\t[AR] $@"
	$(SHOWCL)$(AR) cru $@ $(foreach M,$(LIBMODS_$(patsubst lib/lib%.a,%,$@)),obj/$(M).o)
	$(SHOWCL)$(RANLIB) $@

$(MOD_TARGET): | modules/.d
	@echo -e "\t[LD - MOD] $@"
	$(SHOWCL)$(CC) -shared -o $@ $(@:modules/%.so=obj/%.o) $(LDFLAGS) $(foreach L,$(LIBS) $(MODLIBS_$(patsubst modules/%.so,%,$@)),-l$(L))
	
$(BIN_TARGET): | bin/.d
	@echo -e "\t[LD] $@"
	$(SHOWCL)$(CC) -o $@ $(foreach M,$(MODS_$(notdir $(basename $@))),obj/$(M).o) $(LDFLAGS) $(foreach L,$(LIBS) $(LIBS_$(patsubst bin/%,%,$@)),-l$(L))

##########################################
# Pattern matched build targets.
#

obj/%.o : src/%.c Makefile | obj/.d
	@echo -e "\t[CC] $@"
	$(SHOWCL)$(CC) -x c $(CFLAGS) -o $@ -c $<

obj/%.o : src/%.C Makefile | obj/.d
	@echo -e "\t[CXX] $@"
	$(SHOWCL)$(CC) $(CFLAGS) -o $@ -c $<

deps/%.d : src/%.c Makefile | deps/.d
	@echo -e "\t[DEP] $@"
	$(SHOWCL)$(CC) -x c $(CFLAGS) -o $@ -MG -MM $<
	$(SHOWCL)sed -e 's,$(notdir $*.o):,$@ obj/$*.o:,' -i $@

deps/%.d : src/%.C Makefile | deps/.d
	@echo -e "\t[DEP] $@"
	$(SHOWCL)$(CC) $(CFLAGS) -o $@ -MG -MM $<
ifneq ($(NEED_QT3),)
	$(SHOWCL)sed -re 's, ui_, include/ui_,g' -i $@
endif
	$(SHOWCL)sed -e 's,$(notdir $*.o):,$@ obj/$*.o:,' -i $@

deps/TEX_%.td : tex/%.tex | deps/.d
	@echo -e "\t[TEXIDEP] $@"
	$(SHOWCL)>$@
	$(SHOWCL)echo "TEXDEPS_$*=$<" >> $@
	$(SHOWCL)sed -nre 's:.*\\(input|include)\{(([^.}]|\.[^t}]|\.t[^e}]|\.te[^x}])+)(\.tex)?}.*:\2:p' < $< | sort -u | sed -e 's:.*:-include deps/TEX_\0.td\nTEXDEPS_$*+=$$(TEXDEPS_\0):' >> $@

deps/%.td : | tex/%.tex deps/.d
	@echo -e "\t[TEXDEP] $@"
	$(SHOWCL)>$@
	$(SHOWCL)echo "-include deps/TEX_$*.td" >>$@
	$(SHOWCL)echo 'doc/$*.dvi : $$(TEXDEPS_$*)' >>$@

.PRECIOUS: tex-tmp/%.log
tex-tmp/%.dvi tex-tmp/%.aux tex-tmp/%.log tex-tmp/%.toc : tex/%.tex | tex-tmp/.d
	@echo -e "\t[TEX] $@"
	$(SHOWCL)$(LATEX) -output-directory $(dir $@) -interaction $(TEXMODE) -halt-on-error $<
	$(SHOWCL)$(LATEX) -output-directory $(dir $@) -interaction $(TEXMODE) -halt-on-error $<

doc/%.pdf : tex-tmp/%.dvi | doc/.d
	@echo -e "\t[PDF] $@"
	$(SHOWCL)$(DVIPDF) $< $@

doc/%.ps : tex-tmp/%.dvi | doc/.d
	@echo -e "\t[PS] $@"
	$(SHOWCL)$(DVIPS) -o $@ $<

tex/%_log.tex : tex/%.tex
	@echo -e "\t[TEXLOG] $<"
	$(SHOWCL)echo '\begin{itemize}' >$@
	$(SHOWCL)$(SVN) log $< | sed -nre ' /^-----------*$$/ d' -e 's/^r([0-9]+) \| ([^|]*) \| [^ ]+ ([^ ]+) .*\(([^)]+)\).*$$/\\item Rev \1 by \2 at \3 on \4/p' -e '/^\\item/d' -e 's/^.+$$/\\\\\0/p' >>$@
	$(SHOWCL)echo '\end{itemize}' >>$@

.PRECIOUS: %/.d
%/.d :
	@echo -e "\t[DIR] $(dir $@)"
	$(SHOWCL)mkdir -p $(dir $@)
	@touch $@

ifneq ($(NEED_QT3),)
include/ui_%.h : ui/%.ui
	@echo -e "\t[UIC] $@"
	$(SHOWCL)$(UIC) $< -o $@

src/moc_%.C : include/%.h
	@echo -e "\t[MOC] $@"
	$(SHOWCL)$(MOC) $< -o $@

src/ui_%.C : ui/%.ui
	$(SHOWCL)$(UIC) -impl ui_$*.h $< -o $@

.PRECIOUS : src/ui_%.C src/moc_%.C include/ui_%.h
endif

##########################################
# Dependancy files.  For example, we need a way to tell make
# that objects needs to be brough up to date before the binaries.
#
depfiles := deps/Makefile
deps/Makefile : Makefile | deps/.d
	@echo -e "\t[DEP] $@"
	$(SHOWCL)>$@
	$(SHOWCL)$(foreach B,$(TARGET_BINS),echo "bin/$(B) : $(patsubst %,obj/%.o,$(MODS_$(B)))" >> $@;)
	$(SHOWCL)$(foreach L,$(TARGET_LIBS),echo "lib/lib$(L).so lib/lib$(L).a : $(patsubst %,obj/%.o,$(LIBMODS_$(L)))" >> $@;)
	$(SHOWCL)$(foreach M,$(TARGET_MODS),echo "modules/$(M).so : obj/$(M).o" >> $@;)
	$(SHOWCL)$(if $(filter $(LIBS),$(TARGET_LIBS)),echo "$(strip $(LIB_TARGET_SO) $(BIN_TARGET)) : $(foreach L,$(filter $(LIBS),$(TARGET_LIBS)),lib/lib$(L).so lib/lib$(L).a)" >> $@;,)
	$(SHOWCL)$(foreach B,$(TARGET_BINS),$(if $(filter $(LIBS_$(B)),$(TARGET_LIBS)),echo "bin/$(B) : $(foreach L,$(filter $(LIBS_$(B)),$(TARGET_LIBS)),lib/lib$(L).so lib/lib$(L).a)" >> $@;,))
	$(SHOWCL)$(foreach L,$(TARGET_LIBS),$(if $(filter $(LIBS_$(L)),$(TARGET_LIBS)),echo "lib/lib$(L).so : $(foreach L,$(filter $(LIBS_$(L)),$(TARGET_LIBS)),lib/lib$(L).so)" >> $@;,))
	$(SHOWCL)$(foreach M,$(ALLMODS),echo "obj/$(M).o : | $(dir obj/$(M)).d" >>$@;)

depfiles += $(patsubst %,deps/%.d,$(filter-out ui_% moc_%,$(ALLMODS)))
depfiles += $(patsubst %,deps/%.td,$(TARGET_DOCS))

deps/.d : $(filter-out deps/.d,$(addsuffix .d,$(dir $(depfiles))))

.PHONY: clean
clean :
	@echo -e "\t[RM] deps/"
	$(SHOWCL)rm -rf deps
	@echo -e "\t[RM] obj/"
	$(SHOWCL)rm -rf obj
	@echo -e "\t[RM] include/ui_*.h"
	$(SHOWCL)rm -f include/ui_*.h
	@echo -e "\t[RM] src/mod_*.C"
	$(SHOWCL)rm -f src/moc_*.C
	@echo -e "\t[RM] src/ui_*.C"
	$(SHOWCL)rm -f src/ui_*.C
	@echo -e "\t[RM] ui/*~"
	$(SHOWCL)rm -f ui/*~
	@echo -e "\t[RM] tex-tmp/"
	$(SHOWCL)rm -rf tex-tmp

.PHONY: distclean
distclean : clean
ifneq ($(BIN_TARGET),)
	@echo -e "\t[RM] $(BIN_TARGET)"
	$(SHOWCL)rm -f $(BIN_TARGET)
endif
ifneq ($(DOC_PS),)
	@echo -e "\t[RM] $(DOC_PS)"
	$(SHOWCL)rm -f $(DOC_PS)
endif
ifneq ($(DOC_PDF),)
	@echo -e "\t[RM] $(DOC_PDF)"
	$(SHOWCL)rm -f $(DOC_PDF)
endif
	@echo -e "\t[RM] lib/"
	$(SHOWCL)rm -rf lib
	@echo -e "\t[RM] modules/"
	$(SHOWCL)rm -rf modules

##########################################
# This whole thing is to determine whether we need the depfiles or not.
#
NODEP_TARGETS := clean distclean
depinc := 1
ifneq (,$(filter $(NODEP_TARGETS),$(MAKECMDGOALS)))
depinc := 0
endif
ifneq (,$(fitler-out $(NODEP_TARGETS),$(MAKECMDGOALS)))
depinc := 1
endif

ifeq ($(depinc),1)
#$(error $(depfiles))
-include $(depfiles)
endif
