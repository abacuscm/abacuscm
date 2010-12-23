TARGETS = ../html/abacuscm.html abacuscm.pdf
STYLESHEETS = $(wildcard *.xsl)
SOURCES = $(wildcard *.xml)
CLEANFILES = $(TARGETS) abacuscm.pdf abacuscm.fo

XMLLINT = xmllint
XSLTPROC = xsltproc
FOP = fop

html: ../html/abacuscm.html
pdf: abacuscm.pdf
all: $(TARGETS)

../html/abacuscm.html: $(SOURCES) $(STYLESHEETS) Makefile
	$(XMLLINT) --xinclude --postvalid --noout abacuscm.xml
	$(XSLTPROC) --xinclude -o $@ xhtml.xsl abacuscm.xml

abacuscm.pdf: abacuscm.fo
	$(FOP) -fo $< -pdf $@

abacuscm.fo: $(SOURCES) $(STYLESHEETS) Makefile
	$(XMLLINT) --xinclude --postvalid --noout abacuscm.xml
	$(XSLTPROC) --xinclude -o $@ fo.xsl abacuscm.xml

clean:
	rm -f $(CLEANFILES)

.PHONY: all clean html pdf
