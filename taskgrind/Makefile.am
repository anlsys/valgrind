include $(top_srcdir)/Makefile.tool.am

EXTRA_DIST = docs/taskgrind-manual.xml

#----------------------------------------------------------------------------
# taskgrind-<platform>
#----------------------------------------------------------------------------

pkginclude_HEADERS = taskgrind.h

noinst_PROGRAMS  = taskgrind-@VGCONF_ARCH_PRI@-@VGCONF_OS@
if VGCONF_HAVE_PLATFORM_SEC
noinst_PROGRAMS += taskgrind-@VGCONF_ARCH_SEC@-@VGCONF_OS@
endif

TASKGRIND_TASK_SOURCES_COMMON = main.c env.c dot.c task.c array.c location.c error.c malloc.c malloc_record.c print.c pass/w1.c pass/e1.c

taskgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_SOURCES      = \
	$(TASKGRIND_TASK_SOURCES_COMMON)
taskgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
taskgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CFLAGS       = $(LTO_CFLAGS) \
	$(AM_CFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
taskgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_DEPENDENCIES = \
	$(TOOL_DEPENDENCIES_@VGCONF_PLATFORM_PRI_CAPS@)
taskgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDADD        = \
	$(TOOL_LDADD_@VGCONF_PLATFORM_PRI_CAPS@)
taskgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDFLAGS      = \
	$(TOOL_LDFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
taskgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LINK = \
	$(top_builddir)/coregrind/link_tool_exe_@VGCONF_OS@ \
	@VALT_LOAD_ADDRESS_PRI@ \
	$(LINK) \
	$(taskgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CFLAGS) \
	$(taskgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDFLAGS)

if VGCONF_HAVE_PLATFORM_SEC
taskgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_SOURCES      = \
	$(TASKGRIND_TASK_SOURCES_COMMON)
taskgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
taskgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CFLAGS       = $(LTO_CFLAGS) \
	$(AM_CFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
taskgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_DEPENDENCIES = \
	$(TOOL_DEPENDENCIES_@VGCONF_PLATFORM_SEC_CAPS@)
taskgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDADD        = \
	$(TOOL_LDADD_@VGCONF_PLATFORM_SEC_CAPS@)
taskgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDFLAGS      = \
	$(TOOL_LDFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
taskgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LINK = \
	$(top_builddir)/coregrind/link_tool_exe_@VGCONF_OS@ \
	@VALT_LOAD_ADDRESS_SEC@ \
	$(LINK) \
	$(taskgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CFLAGS) \
	$(taskgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDFLAGS)
endif

#----------------------------------------------------------------------------
# vgpreload_taskgrind-<platform>.so
#----------------------------------------------------------------------------

noinst_PROGRAMS += vgpreload_taskgrind-@VGCONF_ARCH_PRI@-@VGCONF_OS@.so
if VGCONF_HAVE_PLATFORM_SEC
noinst_PROGRAMS += vgpreload_taskgrind-@VGCONF_ARCH_SEC@-@VGCONF_OS@.so
endif

if VGCONF_OS_IS_DARWIN
noinst_DSYMS = $(noinst_PROGRAMS)
endif

vgpreload_taskgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_so_SOURCES      =
vgpreload_taskgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_so_CPPFLAGS     = \
    $(AM_CPPFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
vgpreload_taskgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_so_CFLAGS       = \
    $(AM_CFLAGS_PSO_@VGCONF_PLATFORM_PRI_CAPS@)
vgpreload_taskgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_so_DEPENDENCIES = \
    $(LIBREPLACEMALLOC_@VGCONF_PLATFORM_PRI_CAPS@)
vgpreload_taskgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_so_LDFLAGS      = \
    $(PRELOAD_LDFLAGS_@VGCONF_PLATFORM_PRI_CAPS@) \
    $(LIBREPLACEMALLOC_LDFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)

if VGCONF_HAVE_PLATFORM_SEC
vgpreload_taskgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_so_SOURCES      =
vgpreload_taskgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_so_CPPFLAGS     = \
    $(AM_CPPFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
vgpreload_taskgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_so_CFLAGS       = \
    $(AM_CFLAGS_PSO_@VGCONF_PLATFORM_SEC_CAPS@)
vgpreload_taskgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_so_DEPENDENCIES = \
    $(LIBREPLACEMALLOC_@VGCONF_PLATFORM_SEC_CAPS@)
vgpreload_taskgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_so_LDFLAGS      = \
    $(PRELOAD_LDFLAGS_@VGCONF_PLATFORM_SEC_CAPS@) \
    $(LIBREPLACEMALLOC_LDFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
endif
