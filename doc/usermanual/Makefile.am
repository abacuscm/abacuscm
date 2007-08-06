if DO_MANUAL

doc_DATA = usermanual.pdf
EXTRA_DIST = usermanual.tex

usermanual.pdf: $(srcdir)/usermanual.tex
	$(PDFLATEX) $(srcdir)/usermanual.tex

MOSTLYCLEANFILES = usermanual.out usermanual.log usermanual.aux
CLEANFILES = usermanual.pdf

endif
