#  Copyright (C) 2010-2011  Bruce Merry and Carl Hultquist
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; version 2.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

TOP = ../../../..
TARGET_DIR = ../webapp
MANUAL_DIR = $(TOP)/doc/docbook
MANUAL_FILE = $(TOP)/doc/html/usermanual.html

SOURCES = $(wildcard index*.html) standings.html
TARGETS = $(patsubst %,$(TARGET_DIR)/%,$(SOURCES))
XMLLINT = xmllint
XSLTPROC = xsltproc

all: $(TARGETS)

$(TARGET_DIR)/%.html: %.html merge.xsl body.html $(MANUAL_FILE) Makefile
	$(XMLLINT) --valid --noout $<
	$(XSLTPROC) merge.xsl $< > $@

$(MANUAL_FILE): FORCE
	$(MAKE) -C $(MANUAL_DIR)

clean:
	rm -f $(TARGETS)

FORCE:
.PHONY: all clean FORCE
.DELETE_ON_ERROR:
