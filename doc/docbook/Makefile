TARGETS = ../html/abacuscm.html ../html/usermanual.html abacuscm.pdf
STYLESHEETS = $(wildcard *.xsl)
SOURCES = $(wildcard *.xml)
CLEANFILES = $(TARGETS) abacuscm.pdf abacuscm.fo

XMLLINT = xmllint
XSLTPROC = xsltproc
FOP = fop

html: ../html/abacuscm.html ../html/usermanual.html
pdf: abacuscm.pdf
all: $(TARGETS)

../html/abacuscm.html: $(SOURCES) $(STYLESHEETS) Makefile
	$(XMLLINT) --xinclude --postvalid --noout abacuscm.xml
	$(XSLTPROC) --xinclude -o $@ xhtml.xsl abacuscm.xml

../html/usermanual.html: $(SOURCES) $(STYLESHEETS) Makefile
	$(XMLLINT) --xinclude --postvalid --noout usermanual.xml
	$(XSLTPROC) --xinclude -o $@ xhtml.xsl usermanual.xml

abacuscm.pdf: abacuscm.fo
	$(FOP) -fo $< -pdf $@

abacuscm.fo: $(SOURCES) $(STYLESHEETS) Makefile
	$(XMLLINT) --xinclude --postvalid --noout abacuscm.xml
	$(XSLTPROC) --xinclude -o $@ fo.xsl abacuscm.xml

clean:
	rm -f $(CLEANFILES)

.PHONY: all clean html pdf
