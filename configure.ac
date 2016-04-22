AC_INIT([pty], [1.0], [sbalneav@ltsp.org])

AM_MAINTAINER_MODE

AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/pty.c])

AC_PREREQ([2.59])
AC_PROG_CC

AC_CHECK_LIB(util, forkpty, [UTIL_LIBS="-lutil"],
                 AC_MSG_ERROR([util library missing]))
AC_SUBST(UTIL_LIBS)

AC_CHECK_HEADER([sys/types.h], ,  [AC_MSG_ERROR([sys/types.h headers missing])])
AC_CHECK_HEADER([sys/select.h], , [AC_MSG_ERROR([sys/select.h headers missing])])
AC_CHECK_HEADER([sys/wait.h], ,   [AC_MSG_ERROR([sys/wait.h headers missing])])

AC_CONFIG_FILES([Makefile] [src/Makefile] [man/Makefile])
AC_OUTPUT
