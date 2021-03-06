#                                               -*- Autoconf -*- 
# Process this file with autoconf to produce a configure script. 
# Order is largely irrevellant, although it must start with AC_INIT and end with AC_OUTPUT 
# See http://autotoolset.sourceforge.net/tutorial.html 
# and http://www.openismus.com/documents/linux/automake/automake.shtml 

AC_INIT(SCEADAN, 1.2.1, bugs@afflib.org)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES([Makefile src/Makefile tools/Makefile doc/Makefile liblinear-1.94-omp/Makefile])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AC_PREFIX_PROGRAM(sceadan)

# Programs we will be using
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_CC_C99                  dnl please use C99

m4_include([m4/slg_searchdirs.m4])
m4_include([m4/slg_gcc_all_warnings.m4])
m4_include([m4/ax_openmp.m4])
m4_include([sceadan_configure.m4])

################################################################
# Specify our other headers

AC_HEADER_STDC
AC_CHECK_HEADERS([string.h])

#
# mix-ins for sceadan
#

AC_CHECK_HEADERS([unistd.h])
AC_CHECK_FUNCS([vasprintf])

#
# OpenMP
AX_OPENMP([AC_MSG_NOTICE([OpenMP Found])])
CFLAGS="$CFLAGS $OPENMP_CFLAGS"
CXXFLAGS="$CXXFLAGS $OPENMP_CFLAGS"

# -llinear
AC_CHECK_HEADERS([linear.h liblinear/linear.h])
AC_CHECK_LIB([linear],[load_model],,AC_MSG_ERROR([missing -llinear]))

# -lm
AC_CHECK_HEADERS([math.h],,AC_MSG_ERROR([missing math.h]))
AC_CHECK_LIB([m],[fmax],,AC_MSG_ERROR([missing -lm]))
AC_CHECK_FUNCS([fmin fmax log exp fabs sqrt],,AC_MSG_ERROR([missing math functions]))


############## drop optimization flags if requested ################
############## or if GCC_NOOPT variable is set      ################
AC_ARG_WITH(noopt, AC_HELP_STRING([--with-noopt], [Drop -O C flags]))
if test x"${GCC_NOOPT}" != "x" ; then
   with_noopt="yes";
fi

if test "${with_noopt}" = "yes" ; then
  CFLAGS=`echo "$CFLAGS" | sed s/-O[[0-9]]//`             # note the double quoting! 
  CXXFLAGS=`echo "$CXXFLAGS" | sed s/-O[[0-9]]//`
fi

AC_OUTPUT
if test ! -r src/sceadan_model_precompiled.dat ; then
  echo ''
  echo '***                                                      ***'
  echo '*** type make download to download the precompiled model ***'
  echo '*** (not necessary if you want to build a new model.)    ***'
fi


