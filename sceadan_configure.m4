################################################################
AC_CHECK_HEADERS([linear.h liblinear/linear.h])
liblinear="no"
AC_CHECK_LIB([linear],[load_model],[liblinear="yes"],[AC_MSG_WARN([Sceadan requires liblinear])])

# This file may be in several locations depending on how this file is included
# But don't do it when cross-compiling

if test "$cross_compiling" == no; then
  AC_CHECK_FILES([sceadan_model_precompiled.dat
                  src/sceadan_model_precompiled.dat
                  sceadan/src/sceadan_model_precompiled.dat
                  src/sceadan/src/sceadan_model_precompiled.dat])
fi

# Check which version of liblinear we have
AC_CHECK_MEMBER([struct parameter.p], 
                [AC_DEFINE(LIBLINEAR_19, 1, [We have liblinear 1.9+])],
                [AC_MSG_WARN([liblinear 1.9 NOT detected. Consider upgrading.])],
        [[#include <linear.h>]])



