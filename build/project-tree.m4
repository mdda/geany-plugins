AC_DEFUN([GP_CHECK_PROJECTTREE],
[
    LIBXML_VERSION=2.6.27

    GP_ARG_DISABLE([project-tree], [auto])
    GP_CHECK_PLUGIN_DEPS([project-tree], [LIBXML],
                         [libxml-2.0 >= ${LIBXML_VERSION}])
    GP_COMMIT_PLUGIN_STATUS([Project Tree])

    AC_CONFIG_FILES([
        project-tree/Makefile
        project-tree/src/Makefile
    ])
])
