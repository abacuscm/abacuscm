HTML_TARGETS = ../html/abacuscm-web.html ../html/contestant-web.html ../html/contestant-qt.html
PDF_TARGETS = abacuscm-web.pdf contestant-web.pdf contestant-qt.pdf
TARGETS = $(HTML_TARGETS) $(PDF_TARGETS)
SOURCES = abacuscm.xml \
	  contestant.xml \
	  administrator-core.xml administrator-webapp.xml \
	  developer-core.xml developer-webapp.xml \
	  licensing.xml
CLEANFILES = $(TARGETS) abacuscm-web.xml contestant-web.xml abacuscm-web.fo contestant-web.fo

XMLLINT = xmllint
XSLTPROC = xsltproc
FOP = fop

html: $(HTML_TARGETS)
pdf: $(PDF_TARGETS)
all: $(TARGETS)

abacuscm-%.xml: profile-%.xsl $(SOURCES) Makefile
	$(XMLLINT) --xinclude --postvalid --noout abacuscm.xml
	$(XSLTPROC) --xinclude -o $@ $< abacuscm.xml

contestant-%.xml: profile-%.xsl contestant.xml Makefile
	$(XMLLINT) --xinclude --postvalid --noout contestant.xml
	$(XSLTPROC) --xinclude -o $@ $< contestant.xml

../html/%.html: %.xml xhtml.xsl Makefile
	$(XMLLINT) --postvalid --noout $<
	$(XSLTPROC) --xinclude -o $@ xhtml.xsl $<

%.pdf: %.fo
	$(FOP) -fo $< -pdf $@

%.fo: %.xml fo.xsl Makefile
	$(XMLLINT) --xinclude --postvalid --noout $<
	$(XSLTPROC) --xinclude -o $@ fo.xsl $<

clean:
	rm -f $(CLEANFILES)

.PHONY: all clean html pdf
