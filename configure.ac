dnl
dnl Process this file in order to make a configure script
dnl

dnl Setup autoconf
AC_INIT([runns], [1.4], [coffe92@gmail.com])
dnl AM_INIT_AUTOMAKE

dnl Debug option
AC_ARG_ENABLE(debug,
              AS_HELP_STRING([--enable-debug],[enable debugging, defult: no]),
              [case "${enableval}" in
               yes) debug=true  ;;
               no)  debug=false ;;
               *)   AC_MSG_ERROR([bad value ${enablevalue} for --enable-debug]) ;;
             esac], [debug=false])

dnl Check dependences
echo "Cheking dependences"
AC_LANG(C)
AC_CHECK_TOOL(IPTABLES, iptables, "", PATH="$PATH:/sbin")
AC_CHECK_TOOL(IP, ip)
AC_CHECK_TOOL(GAWK, gawk)

dnl Disable all CFLAGS if it was not declarated
if [ test -z "$CFLAGS"; ] then
  if [ test "x$debug" == "xtrue" ]; then
    CFLAGS="-g -O0"
  else
    CFLAGS="-O2"
  fi
fi

AC_PROG_CC

if [ test "x$IPTABLES" == "x"; ] then
  AC_MSG_ERROR([iptables is required])
fi

if [ test "x$IP" == "x"; ] then
  AC_MSG_ERROR([ip from iproute2 is required])
fi

if [ test "x$GAWK" == "x"; ] then
  AC_MSG_ERROR([gawk is required])
fi

dnl Get source code path
dnl AC_DEFINE_UNQUOTED([SRC_PATH], ["$srcdir"], [source path])

dnl Configure Makefile from Makefile.in
AC_CONFIG_FILES([Makefile])

AC_OUTPUT
