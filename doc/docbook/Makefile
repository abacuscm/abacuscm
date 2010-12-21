TARGETS = ../html/abacuscm.html
STYLESHEETS = $(wildcard *.xsl)
SOURCES = $(wildcard *.xml)

XMLLINT = xmllint
XSLTPROC = xsltproc

all: $(TARGETS)

../html/abacuscm.html: $(SOURCES) $(STYLESHEETS) Makefile
	$(XMLLINT) --xinclude --postvalid --noout abacuscm.xml
	$(XSLTPROC) --xinclude -o $@ xhtml.xsl abacuscm.xml

clean:
	rm -f $(TARGETS)

.PHONY: all clean
