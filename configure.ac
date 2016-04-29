dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)

AC_INIT([siphon], [0.0.1])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([src/siphon.c])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

AC_CHECK_LIB(m, acos,
  [],
  [AC_MSG_ERROR([m libraries not found.])]
)

AC_CHECK_LIB(tcl8.6, Tcl_Access,
  [],
  [AC_MSG_ERROR([tcl libraries not found.])]
)

AC_CHECK_LIB(expect, exp_spawnv,
  [],
  [AC_MSG_ERROR([expect libraries not found.])]
)

# Checks for header files.

AC_CHECK_HEADERS(tcl8.6/expect.h,
  [],
  [AC_MSG_ERROR([expect header files not found.]); break]
)

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_OUTPUT(Makefile src/Makefile)
