MAINTAINERCLEANFILES = Makefile.in default.edj

EXTRA_DIST = \
*.edc \
../images/* \
../default_elementary/*.edc \
../default_elementary/images/* \
fonts/*.ttf

EDJE_CC = @edje_cc@
EDJ = default_small.edj

default_small.edj:   *.edc \
../images/* \
../default_elementary/*.edc \
../default_elementary/images/*
	$(EDJE_CC) default.edc -id ../images/ -id ../default_elementary/ default_small.edj

icondir = $(pkgdatadir)/themes
icon_DATA =  $(EDJ)

clean-local:
	rm -rf *.edj *~

uninstall:
	rm -rf $(DESTDIR)$(pkgdatadir)/themes

