include $(top_srcdir)/Makefile.tool.am

EXTRA_DIST = docs/omp_task-manual.xml

#----------------------------------------------------------------------------
# omp_task-<platform>
#----------------------------------------------------------------------------

noinst_PROGRAMS  = omp_task-@VGCONF_ARCH_PRI@-@VGCONF_OS@
if VGCONF_HAVE_PLATFORM_SEC
noinst_PROGRAMS += omp_task-@VGCONF_ARCH_SEC@-@VGCONF_OS@
endif

OMP_TASK_SOURCES_COMMON = omp_task_main.c

omp_task_@VGCONF_ARCH_PRI@_@VGCONF_OS@_SOURCES      = \
	$(OMP_TASK_SOURCES_COMMON)
omp_task_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
omp_task_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CFLAGS       = $(LTO_CFLAGS) \
	$(AM_CFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
omp_task_@VGCONF_ARCH_PRI@_@VGCONF_OS@_DEPENDENCIES = \
	$(TOOL_DEPENDENCIES_@VGCONF_PLATFORM_PRI_CAPS@)
omp_task_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDADD        = \
	$(TOOL_LDADD_@VGCONF_PLATFORM_PRI_CAPS@)
omp_task_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDFLAGS      = \
	$(TOOL_LDFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
omp_task_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LINK = \
	$(top_builddir)/coregrind/link_tool_exe_@VGCONF_OS@ \
	@VALT_LOAD_ADDRESS_PRI@ \
	$(LINK) \
	$(omp_task_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CFLAGS) \
	$(omp_task_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDFLAGS)

if VGCONF_HAVE_PLATFORM_SEC
omp_task_@VGCONF_ARCH_SEC@_@VGCONF_OS@_SOURCES      = \
	$(OMP_TASK_SOURCES_COMMON)
omp_task_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
omp_task_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CFLAGS       = $(LTO_CFLAGS) \
	$(AM_CFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
omp_task_@VGCONF_ARCH_SEC@_@VGCONF_OS@_DEPENDENCIES = \
	$(TOOL_DEPENDENCIES_@VGCONF_PLATFORM_SEC_CAPS@)
omp_task_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDADD        = \
	$(TOOL_LDADD_@VGCONF_PLATFORM_SEC_CAPS@)
omp_task_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDFLAGS      = \
	$(TOOL_LDFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
omp_task_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LINK = \
	$(top_builddir)/coregrind/link_tool_exe_@VGCONF_OS@ \
	@VALT_LOAD_ADDRESS_SEC@ \
	$(LINK) \
	$(omp_task_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CFLAGS) \
	$(omp_task_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDFLAGS)
endif


