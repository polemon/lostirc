AC_INIT
AC_CONFIG_SRCDIR([src/gtkmm/main.cpp])
AM_INIT_AUTOMAKE(lostirc, 0.2.8pre)
AM_CONFIG_HEADER(config.h)
AC_PROG_CXX
AC_PROG_INSTALL
AM_PROG_LIBTOOL
PKG_CHECK_MODULES(DEPS, gtkmm-2.0 >= 2.0.0)
AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

ALL_LINGUAS="da"
AM_GNU_GETTEXT_VERSION(0.11.5)
AM_GNU_GETTEXT

CXXFLAGS="$CXXFLAGS -Wall"

# Add ./configure --enable-logdebug
AC_ARG_ENABLE(logdebug,
        [  --enable-logdebug       compile with log-debugging to stdout],
        logdebug=$enable_logdebug,
        logdebug=no
)
AC_MSG_CHECKING(whether to log debug info to stdout)
if test "x$logdebug" = xyes; then
        AC_MSG_RESULT(yes)
        AC_DEFINE(DEBUG, [], whether to log log debug info to stdout)
else
        AC_MSG_RESULT(no)
fi

# Add ./configure --enable-kde
AC_ARG_ENABLE(kde,
        [  --disable-kde           don't install KDE .desktop/menu entry],
        kde=$enable_kde,
        kde=yes
)
AC_MSG_CHECKING(whether to install KDE .desktop/menu entry)
if test "x$kde" = xyes; then
        AC_MSG_RESULT(yes)
        AM_CONDITIONAL(KDE, true)
else
        AC_MSG_RESULT(no)
        AM_CONDITIONAL(KDE, false)
fi

# Add ./configure --enable-gnome
AC_ARG_ENABLE(gnome,
        [  --disable-gnome         don't install GNOME .desktop/menu entry],
        gnome=$enable_gnome,
        gnome=yes
)
AC_MSG_CHECKING(whether to install GNOME .desktop/menu entry)
if test "x$gnome" = xyes; then
        AC_MSG_RESULT(yes)
        AM_CONDITIONAL(GNOME, true)
else
        AC_MSG_RESULT(no)
        AM_CONDITIONAL(GNOME, false)
fi

dnl Check for socklen_t (it should be int on Windows). Taken from gaim
dnl configure.in.
AC_MSG_CHECKING(for socklen_t)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
        #include <sys/types.h>
        #include <sys/socket.h>
        socklen_t x;
]], [[]])],[
        AC_MSG_RESULT(yes)
],[
        _au_changequote([,])AC_TRY_COMPILE([
                #include <sys/types.h>
                #include <sys/socket.h>
                int accept(int, struct sockaddr *, size_t *);
        ], [], [
                AC_MSG_RESULT(size_t)
                AC_DEFINE(socklen_t, size_t, [socklen_t size])
        ], [
                AC_MSG_RESULT(int)
                AC_DEFINE(socklen_t, int, [socklen_t size])
        ])
])


AC_CONFIG_FILES([lostirc.spec Makefile src/Makefile src/libirc/Makefile src/gtkmm/Makefile po/Makefile.in m4/Makefile  intl/Makefile ])
AC_OUTPUT