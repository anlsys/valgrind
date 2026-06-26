include $(top_srcdir)/Makefile.tool.am

# Private headers (not installed, but shipped in the source distribution).
EXTRA_DIST = \
	print.h \
	tracegrind_spmt.h \
	spmt/spmt.h \
	README.md

#----------------------------------------------------------------------------
# Header that gets installed.
#----------------------------------------------------------------------------

pkginclude_HEADERS = tracegrind.h

#----------------------------------------------------------------------------
# tracegrind-<platform>
#----------------------------------------------------------------------------

noinst_PROGRAMS  = tracegrind-@VGCONF_ARCH_PRI@-@VGCONF_OS@
if VGCONF_HAVE_PLATFORM_SEC
noinst_PROGRAMS += tracegrind-@VGCONF_ARCH_SEC@-@VGCONF_OS@
endif

TRACEGRIND_SOURCES_COMMON = main.c print.c

tracegrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_SOURCES      = \
	$(TRACEGRIND_SOURCES_COMMON)
tracegrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
tracegrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CFLAGS       = $(LTO_CFLAGS) \
	$(AM_CFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
tracegrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_DEPENDENCIES = \
	$(TOOL_DEPENDENCIES_@VGCONF_PLATFORM_PRI_CAPS@)
tracegrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDADD        = \
	$(TOOL_LDADD_@VGCONF_PLATFORM_PRI_CAPS@)
tracegrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDFLAGS      = \
	$(TOOL_LDFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
tracegrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LINK = \
	$(top_builddir)/coregrind/link_tool_exe_@VGCONF_OS@ \
	@VALT_LOAD_ADDRESS_PRI@ \
	$(LINK) \
	$(tracegrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CFLAGS) \
	$(tracegrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDFLAGS)

if VGCONF_HAVE_PLATFORM_SEC
tracegrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_SOURCES      = \
	$(TRACEGRIND_SOURCES_COMMON)
tracegrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
tracegrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CFLAGS       = $(LTO_CFLAGS) \
	$(AM_CFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
tracegrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_DEPENDENCIES = \
	$(TOOL_DEPENDENCIES_@VGCONF_PLATFORM_SEC_CAPS@)
tracegrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDADD        = \
	$(TOOL_LDADD_@VGCONF_PLATFORM_SEC_CAPS@)
tracegrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDFLAGS      = \
	$(TOOL_LDFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
tracegrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LINK = \
	$(top_builddir)/coregrind/link_tool_exe_@VGCONF_OS@ \
	@VALT_LOAD_ADDRESS_SEC@ \
	$(LINK) \
	$(tracegrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CFLAGS) \
	$(tracegrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDFLAGS)
endif
