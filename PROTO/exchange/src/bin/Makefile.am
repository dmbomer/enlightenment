MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -Wall \
-I. \
-I$(top_srcdir)/src/lib \
@EXCHANGE_CFLAGS@ \
-DPACKAGE_BIN_DIR=\"$(bindir)\" \
-DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(datadir)/$(PACKAGE)\"

#bin_PROGRAMS = exchange_cli

#exchange_cli_DEPENDENCIES = $(top_builddir)/src/lib/libexchange.la
#exchange_cli_LDADD = $(top_builddir)/src/lib/libexchange.la \
#                    @EXCHANGE_LIBS@
#exchange_cli_SOURCES = \
#exchange_cli.c
