AC_INIT([pty], [1.0], [sbalneav@ltsp.org])
AC_CONFIG_HEADER(config.h)

AM_MAINTAINER_MODE

AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/pty.c])

AC_PREREQ([2.59])
AC_PROG_CC
AM_PROG_CC_C_O

AC_CHECK_HEADER([termios.h], , [AC_MSG_ERROR([termios.h headers missing])])
AC_CONFIG_FILES([Makefile] [src/Makefile])
AC_OUTPUT

