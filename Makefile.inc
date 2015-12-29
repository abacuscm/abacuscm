# Should be included from other Makefiles.

# 0 == debug off, anything else switches DEBUG on
DEBUG ?= 1
STATIC ?= 0

TOPDIR ?= $(PWD)

# Figure out PKGNAME - override from elsewhere if possible.
PKGDIRNAME = $(lastword $(subst /, ,$(shell readlink -f $(TOPDIR))))
PKGNAME ?= $(shell echo "$(PKGDIRNAME)" | sed -re 's/(-[0-9]+)*$$//')
PKGVERSION ?= $(shell echo "$(PKGDIRNAME)" | sed -nre 's/^([^-]|-[0-9]*[^0-9-])+-([0-9]+(-[0-9]+)?)$$/\2/p')

# This is a C++ based lib.  End of story.
CC := g++
AR := ar
RANLIB ?= ranlib
ECHO_E ?= /bin/echo -e   # Don't just use echo: in dash it doesn't process -e

CFLAGS ?= -W -Wall
LDFLAGS ?=

CFLAGS += -Iinclude
LDFLAGS += -rdynamic -Llib

OPTIM ?= -O2

XSLTPROC ?= xsltproc
XMLLINT ?= xmllint
FOP ?= fop

ifneq ($(EXTRA_MODULE_DIRS),)
CFLAGS += $(foreach D,$(EXTRA_MODULE_DIRS),-I$(D)/include)
LDFLAGS += $(foreach D,$(EXTRA_MODULE_DIRS),-L$(D)/lib)
endif

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

RPATHDIR ?= $(EXTRA_RPATHDIR) $(PWD)/lib

INCDIRS := $(shell [ -d include ] && find include -type d | sed -e 's:^include::')

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
SBINDIR ?= $(PREFIX)/sbin
LIBDIR ?= $(PREFIX)/lib
INCDIR ?= $(PREFIX)/include
MODDIR ?= $(PREFIX)/lib/$(PKGNAME)
ETCDIR ?= $(PREFIX)/etc

DESTDIR ?= /

DEFS += -DPREFIX=\"$(PREFIX)\" -DSYSCONFDIR=\"$(ETCDIR)\" -DMODDIR=\"$(MODDIR)\" -DBINDIR=\"$(BINDIR)\"

-include $(TOPDIR)/Makefile.extra

##############################################################################################
#### From here on there should be no reason to change unless you know what you are doing. ####
##############################################################################################
.PHONY: default
default: all

LIB_TARGET_SO := $(foreach L,$(TARGET_LIBS),lib/lib$(L).so)
LIB_TARGET_A := $(foreach L,$(TARGET_LIBS),lib/lib$(L).a)
BIN_TARGET := $(foreach B,$(TARGET_BINS),bin/$(B))
MOD_TARGET := $(foreach M,$(TARGET_MODS),modules/$(M).so)
PDF_TARGET := $(foreach D,$(TARGET_DOCS),doc/pdf/$(D).pdf)
HTML_TARGET := $(foreach D,$(TARGET_DOCS),doc/html/$(D).html)

ALL_TARGET :=
ALL_TARGET += $(LIB_TARGET_SO)
ALL_TARGET += $(LIB_TARGET_A)
ALL_TARGET += $(BIN_TARGET)
ALL_TARGET += $(MOD_TARGET)
ALL_TARGET += $(PDF_TARGET)
ALL_TARGET += $(HTML_TARGET)

BINMODS := $(foreach B,$(TARGET_BINS),$(MODS_$(B)))
LIBMODS := $(foreach L,$(TARGET_LIBS),$(MODS_$(L)))
MODMODS := $(foreach M,$(TARGET_MODS),$M)
ALLMODS := $(BINMODS) $(LIBMODS) $(MODMODS)

$(LIBMODS:%=obj/%.o) $(MODMODS:%=obj/%.o): CFLAGS += -fpic

.PHONY: all
all: $(ALL_TARGET)
	@echo "Everything has been brought up to date"

.PHONY: docs pdfdoc htmldoc
pdfdoc: $(PDF_TARGET)
htmldoc: $(HTML_TARGET)
docs: htmldoc pdfdoc

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

ifneq ($(RPATHDIR),)
LDFLAGS += $(foreach RD,$(RPATHDIR),-Wl,-rpath,$(shell readlink -f "$(RD)"))
endif

ifneq ($(VERBOSE),)
SHOWCL=
else
SHOWCL=@
endif

##########################################
# How to build actual binaries/libraries
#
$(LIB_TARGET_SO): | lib/.d
	@$(ECHO_E) "\t[LD - SO] $@"
	$(SHOWCL)$(CC) -shared -o $@ $(foreach M,$(LIBMODS_$(patsubst lib/lib%.so,%,$@)),obj/$(M).o) $(LDFLAGS) $(foreach L,$(LIBS) $(LIBS_$(patsubst lib/lib%.so,%,$@)),-l$(L))


$(LIB_TARGET_A): | lib/.d
	@$(ECHO_E) "\t[AR] $@"
	$(SHOWCL)$(AR) cru $@ $(foreach M,$(LIBMODS_$(patsubst lib/lib%.a,%,$@)),obj/$(M).o)
	$(SHOWCL)$(RANLIB) $@

$(MOD_TARGET): | modules/.d
	@$(ECHO_E) "\t[LD - MOD] $@"
	$(SHOWCL)$(CC) -shared -o $@ $(@:modules/%.so=obj/%.o) $(LDFLAGS) $(foreach L,$(LIBS) $(MODLIBS_$(patsubst modules/%.so,%,$@)),-l$(L))
	
$(BIN_TARGET): | bin/.d
	@$(ECHO_E) "\t[LD] $@"
	$(SHOWCL)$(CC) -o $@ $(foreach M,$(MODS_$(notdir $(basename $@))),obj/$(M).o) $(LDFLAGS) $(foreach L,$(LIBS) $(LIBS_$(patsubst bin/%,%,$@)),-l$(L))

##########################################
# Pattern matched build targets.
#

obj/%.o : src/%.c Makefile | obj/.d
	@$(ECHO_E) "\t[CC] $@"
	$(SHOWCL)$(CC) -x c $(CFLAGS) -o $@ -c $<

obj/%.o : src/%.C Makefile | obj/.d
	@$(ECHO_E) "\t[CXX] $@"
	$(SHOWCL)$(CC) $(CFLAGS) -o $@ -c $<

deps/%.d : src/%.c Makefile | deps/.d
	@$(ECHO_E) "\t[DEP] $@"
	$(SHOWCL)$(CC) -x c $(CFLAGS) -o $@ -MG -MM $<
	$(SHOWCL)sed -e 's,$(notdir $*.o):,$@ obj/$*.o:,' -i $@

deps/%.d : src/%.C Makefile | deps/.d
	@$(ECHO_E) "\t[DEP] $@"
	$(SHOWCL)$(CC) $(CFLAGS) -o $@ -MG -MM $<
	$(SHOWCL)sed -e 's,$(notdir $*.o):,$@ obj/$*.o:,' -i $@

# Fop changed behaviour between 1.0 and 1.1 as to how a <base> tag in the
# configuration file is interpreted: in 1.0 it is taken relative to the current
# directory, while in fop 1.1 is is relative to one of the files (I haven't
# checked which one). Changing directory is thus necessary to ensure that the
# rule works with both fop versions.
doc/pdf/%.pdf: doc/tmp/%.fo | doc/pdf/.d
	@$(ECHO_E) "\t[PDF] $@"
	$(SHOWCL)cd doc/docbook && $(FOP) -q -c fop.xconf -fo $(abspath $<) -pdf $(abspath $@)

doc/tmp/%.fo: doc/tmp/%.xml
	@$(ECHO_E) "\t[FO] $@"
	$(SHOWCL)$(XMLLINT) --postvalid --noout $<
	$(SHOWCL)$(XSLTPROC) --xinclude --path doc/docbook -o $@ doc/docbook/$(PDF_STYLESHEET_$(notdir $(basename $@))).xsl $<

doc/tmp/%.xml: | doc/tmp/.d
	@$(ECHO_E) "\t[XSLT] $@"
	$(SHOWCL)$(XMLLINT) --xinclude --postvalid --noout $<
	$(SHOWCL)$(XSLTPROC) --xinclude --path doc/docbook $(XSLTPROC_ARGS_$(notdir $(basename $@))) -o $@ doc/docbook/profile.xsl $<

doc/html/%.html: doc/tmp/%.xml
	@$(ECHO_E) "\t[HTML] $@"
	$(SHOWCL)$(XMLLINT) --postvalid --noout $<
	$(SHOWCL)$(XSLTPROC) --xinclude --path doc/docbook -o $@ doc/docbook/$(HTML_STYLESHEET_$(notdir $(basename $@))).xsl $<

.PRECIOUS: %/.d
%/.d :
	@$(ECHO_E) "\t[DIR] $(dir $@)"
	$(SHOWCL)mkdir -p $(dir $@)
	@touch $@

##########################################
# Dependency files.  For example, we need a way to tell make
# that objects needs to be brought up to date before the binaries.
#
depfiles := deps/Makefile
deps/Makefile : Makefile | deps/.d
	@$(ECHO_E) "\t[DEP] $@"
	$(SHOWCL)>$@
	$(SHOWCL)$(foreach B,$(TARGET_BINS),echo "bin/$(B) : $(patsubst %,obj/%.o,$(MODS_$(B)))" >> $@;)
	$(SHOWCL)$(foreach L,$(TARGET_LIBS),echo "lib/lib$(L).so lib/lib$(L).a : $(patsubst %,obj/%.o,$(LIBMODS_$(L)))" >> $@;)
	$(SHOWCL)$(foreach M,$(TARGET_MODS),echo "modules/$(M).so : obj/$(M).o" >> $@;)
	$(SHOWCL)$(if $(filter $(LIBS),$(TARGET_LIBS)),echo "$(strip $(LIB_TARGET_SO) $(BIN_TARGET)) : $(foreach L,$(filter $(LIBS),$(TARGET_LIBS)),lib/lib$(L).so lib/lib$(L).a)" >> $@;,)
	$(SHOWCL)$(foreach B,$(TARGET_BINS),$(if $(filter $(LIBS_$(B)),$(TARGET_LIBS)),echo "bin/$(B) : $(foreach L,$(filter $(LIBS_$(B)),$(TARGET_LIBS)),lib/lib$(L).so lib/lib$(L).a)" >> $@;,))
	$(SHOWCL)$(foreach L,$(TARGET_LIBS),$(if $(filter $(LIBS_$(L)),$(TARGET_LIBS)),echo "lib/lib$(L).so : $(foreach L,$(filter $(LIBS_$(L)),$(TARGET_LIBS)),lib/lib$(L).so)" >> $@;,))
	$(SHOWCL)$(foreach M,$(ALLMODS),echo "obj/$(M).o : | $(dir obj/$(M)).d" >>$@;)
	$(SHOWCL)$(foreach D,$(TARGET_DOCS),echo "doc/tmp/$(D).fo : $(patsubst %,doc/docbook/%.xsl,$(PDF_STYLESHEET_$(D)))" >> $@;)
	$(SHOWCL)$(foreach D,$(TARGET_DOCS),echo "doc/tmp/$(D).xml : $(patsubst %,doc/docbook/%.xml,$(XML_$(D))) $(patsubst %,doc/docbook/%.xsl,$(STYLESHEET_$(D)))" >> $@;)

depfiles += $(patsubst %,deps/%.d,$(ALLMODS))

deps/.d : $(filter-out deps/.d,$(addsuffix .d,$(dir $(depfiles))))

.PHONY: clean
clean :
	@$(ECHO_E) "\t[RM] deps/"
	$(SHOWCL)rm -rf deps
	@$(ECHO_E) "\t[RM] obj/"
	$(SHOWCL)rm -rf obj
	@$(ECHO_E) "\t[RM] doc/tmp"
	$(SHOWCL)rm -rf doc/tmp

.PHONY: distclean
distclean : clean
ifneq ($(BIN_TARGET),)
	@$(ECHO_E) "\t[RM] $(BIN_TARGET)"
	$(SHOWCL)rm -f $(BIN_TARGET)
endif
	@$(ECHO_E) "\t[RM] lib/"
	$(SHOWCL)rm -rf lib
	@$(ECHO_E) "\t[RM] modules/"
	$(SHOWCL)rm -rf modules
ifneq ($(PDF_TARGET),)
	@$(ECHO_E) "\t[RM] $(PDF_TARGET)"
	$(SHOWCL)rm -f $(PDF_TARGET)
endif
ifneq ($(HTML_TARGET),)
	@$(ECHO_E) "\t[RM] $(HTML_TARGET)"
	$(SHOWCL)rm -f $(HTML_TARGET)
endif

.PHONY: install
install : all
	@echo "Preparing to install into $(DESTDIR)."
ifneq ($(BIN_TARGET)$(EXTRA_BINFILES),)
	@$(ECHO_E) "\t[DIR] $(BINDIR)"
	$(SHOWCL)install -d "$(DESTDIR)$(BINDIR)"
endif
ifneq ($(BIN_TARGET),)
	$(SHOWCL)$(foreach B,$(BIN_TARGET),$(ECHO_E) "\t[BIN] $(B)"; install $(B) "$(DESTDIR)$(BINDIR)";)
endif
ifneq ($(EXTRA_BINFILES),)
	$(SHOWCL)$(foreach B,$(EXTRA_BINFILES),$(ECHO_E) "\t[BIN] $(B)"; install $(B) "$(DESTDIR)$(BINDIR)";)
endif
ifneq ($(LIB_TARGET_SO),)
	@$(ECHO_E) "\t[DIR] $(LIBDIR)"
	$(SHOWCL)install -d "$(DESTDIR)$(LIBDIR)"
	$(SHOWCL)$(foreach L,$(LIB_TARGET_SO),$(ECHO_E) "\t[LIB] $(L)"; install "$(L)" "$(DESTDIR)$(LIBDIR)";)
	$(SHOWCL)$(foreach L,$(LIB_TARGET_A),$(ECHO_E) "\t[LIB] $(L)"; install -m0644 "$(L)" "$(DESTDIR)$(LIBDIR)";)
endif
ifneq ($(INCDIRS),)
	$(SHOWCL)$(foreach D,$(INCDIRS),$(ECHO_E) "\t[DIR] $(INCDIR)$(D)"; install -d "$(DESTDIR)$(INCDIR)$(D)"; $(foreach F,$(shell find "include$(D)" -maxdepth 1 -type f),$(ECHO_E) "\t[INC] $(F)"; install -m0644 "$(F)" "$(DESTDIR)$(INCDIR)$(D)";))
endif
ifneq ($(MOD_TARGET),)
	@$(ECHO_E) "\t[DIR] $(MODDIR)"
	$(SHOWCL)install -d "$(DESTDIR)$(MODDIR)"
	$(SHOWCL)$(foreach M,$(MOD_TARGET),$(ECHO_E) "\t[MOD] $(M)"; install $(M) "$(DESTDIR)$(MODDIR)";)
endif
	@echo "All installed."

##########################################
# This whole thing is to determine whether we need the depfiles or not.
#
NODEP_TARGETS += clean distclean
depinc := 1
ifneq (,$(filter $(NODEP_TARGETS),$(MAKECMDGOALS)))
depinc := 0
endif
ifneq (,$(filter-out $(NODEP_TARGETS),$(MAKECMDGOALS)))
depinc := 1
endif

ifeq ($(depinc),1)
-include $(depfiles)
endif
