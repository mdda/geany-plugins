include $(top_srcdir)/build/vars.build.mk
plugin = project-tree

geanyplugins_LTLIBRARIES = project-tree.la

project_tree_la_SOURCES = \
	project-tree.h \
	project-tree.c \
	sidebar.h \
	sidebar.c \
	project.h \
	project.c

project_tree_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"ProjectTree\"
project_tree_la_CFLAGS = $(AM_CFLAGS)
project_tree_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk