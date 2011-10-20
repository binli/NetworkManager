
pkglib_LTLIBRARIES = libnm-settings-plugin-ifcfg-suse.la

noinst_LTLIBRARIES = libifcfg-suse-io.la

libifcfg_suse_io_la_SOURCES = \
	shvar.c \
	shvar.h \
	reader.c \
	reader.h \
	errors.c \
	common.h \
	utils.c \
	utils.h

INCLUDES = \
	-I${top_srcdir}/src/settings \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/libnm-glib \
	-I$(top_srcdir)/libnm-util

libifcfg_suse_io_la_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(NSS_CFLAGS) \
	-DG_DISABLE_DEPRECATED \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-DSBINDIR=\"$(sbindir)\"

libifcfg_suse_io_la_LIBADD = \
	$(top_builddir)/libnm-util/libnm-util.la \
	$(GLIB_LIBS) \
	$(NSS_LIBS)

libnm_settings_plugin_ifcfg_suse_la_SOURCES = \
	plugin.c \
	plugin.h \
	nm-ifcfg-suse-connection.c \
	nm-ifcfg-suse-connection.h

libnm_settings_plugin_ifcfg_suse_la_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(DBUS_CFLAGS) \
	-DG_DISABLE_DEPRECATED \
	-DSYSCONFDIR=\"$(sysconfdir)\"

libnm_settings_plugin_ifcfg_suse_la_LDFLAGS = -module -avoid-version
libnm_settings_plugin_ifcfg_suse_la_LIBADD = \
	$(top_builddir)/libnm-util/libnm-util.la \
	$(top_builddir)/libnm-glib/libnm-glib.la \
	libifcfg-suse-io.la \
	$(GLIB_LIBS) \
	$(GMODULE_LIBS) \
	$(GIO_LIBS)

