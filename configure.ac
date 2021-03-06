#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([profanity], [0.5.0], [boothj5web@gmail.com])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([src/config.h])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AX_PREFIX_CONFIG_H([src/prof_config.h], [PROF], [src/config.h])

### Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

### Get canonical host
AC_CANONICAL_HOST
PLATFORM="unknown"
AS_CASE([$host_os],
    [freebsd*], [PLATFORM="freebsd"],
    [darwin*], [PLATFORM="osx"],
    [cygwin], [PLATFORM="cygwin"],
    [PLATFORM="nix"])

PACKAGE_STATUS="development"

### Get git branch and revision if in development
if test "x$PACKAGE_STATUS" = xdevelopment; then
    AM_CONDITIONAL([INCLUDE_GIT_VERSION], [true])
    AC_DEFINE([HAVE_GIT_VERSION], [1], [Include git info])
else
    AM_CONDITIONAL([INCLUDE_GIT_VERSION], [false])
fi

AC_DEFINE_UNQUOTED([PACKAGE_STATUS], ["$PACKAGE_STATUS"], [Status of this build])

AS_IF([test "x$PLATFORM" = xcygwin],
    [AC_DEFINE([PLATFORM_CYGWIN], [1], [Cygwin])])
AS_IF([test "x$PLATFORM" = xosx],
    [AC_DEFINE([PLATFORM_OSX], [1], [OSx])])

### Options
AC_ARG_ENABLE([notifications],
    [AS_HELP_STRING([--enable-notifications], [enable desktop notifications])])
AC_ARG_ENABLE([ruby-plugins],
    [AS_HELP_STRING([--enable-ruby-plugins], [enable Ruby plugins])])
AC_ARG_ENABLE([python-plugins],
    [AS_HELP_STRING([--enable-python-plugins], [enable Python plugins])])
AC_ARG_ENABLE([lua-plugins],
    [AS_HELP_STRING([--enable-lua-plugins], [enable Lua plugins])])
AC_ARG_ENABLE([c-plugins],
    [AS_HELP_STRING([--enable-c-plugins], [enable C plugins])])
AC_ARG_ENABLE([plugins],
    [AS_HELP_STRING([--enable-plugins], [enable plugins])])
AC_ARG_ENABLE([otr],
    [AS_HELP_STRING([--enable-otr], [enable otr encryption])])
AC_ARG_WITH([libxml2],
    [AS_HELP_STRING([--with-libxml2], [link with libxml2 instead of expat])])
AC_ARG_WITH([xscreensaver],
    [AS_HELP_STRING([--with-xscreensaver], [use libXScrnSaver to determine idle time])])
AC_ARG_WITH([themes],
    [AS_HELP_STRING([--with-themes[[=PATH]]], [install themes (default yes)])])

### plugins

# ruby
if test "x$enable_plugins" = xno; then
    AM_CONDITIONAL([BUILD_RUBY_API], [false])
elif test "x$enable_ruby_plugins" != xno; then
    AC_CHECK_PROG(RUBY_CMD_EXISTS, ruby, yes, no)
    if test "$RUBY_CMD_EXISTS" == "yes"; then
        ac_mkmf_result=`ruby -rmkmf -e ";" 2>&1`
        if test -z "$ac_mkmf_result"; then
            AC_MSG_CHECKING(Ruby rbconfig)
                ruby_rbconfig="RbConfig"
                if ! ruby -r rbconfig -e 'RbConfig' >/dev/null 2>/dev/null; then
	                ruby_rbconfig="Config"
                fi
            AC_MSG_RESULT($ruby_rbconfig)
            
            AC_MSG_CHECKING(Ruby cflags)
            rubyhdrdir=`ruby -r mkmf -e "print $ruby_rbconfig::CONFIG[['rubyhdrdir']] || $ruby_rbconfig::CONFIG[['archdir']]" 2>/dev/null`
            if test "X$rubyhdrdir" != "X"; then
                RUBY_CFLAGS="-I$rubyhdrdir"
                rubyarch=`ruby -r rbconfig -e "print $ruby_rbconfig::CONFIG[['arch']]"`
                if test -d "$rubyhdrdir/$rubyarch"; then
                    RUBY_CFLAGS="$RUBY_CFLAGS -I$rubyhdrdir/$rubyarch"
                fi
                AC_MSG_RESULT($RUBY_CFLAGS)

                AC_MSG_CHECKING(Ruby libs)
	            rubylibs=`ruby -r rbconfig -e "print $ruby_rbconfig::CONFIG[['LIBS']]"`
	            RUBY_LIBS="$rubylibs"
	            librubyarg=`ruby -r rbconfig -e "print $ruby_rbconfig.expand($ruby_rbconfig::CONFIG[['LIBRUBYARG']])"`
	            RUBY_LIBS="$librubyarg $RUBY_LIBS"
	            rubylibdir=`ruby -r rbconfig -e "print $ruby_rbconfig.expand($ruby_rbconfig::CONFIG[['libdir']])"`
	            RUBY_LIBS="$RUBY_LIBS -L$rubylibdir"
                AC_MSG_RESULT($RUBY_LIBS)

                AC_MSG_CHECKING(Ruby ld flags)
	            rubyldflags=`ruby -r rbconfig -e "print $ruby_rbconfig::CONFIG[['LDFLAGS']]"`
	            RUBY_LDFLAGS="$rubyldflags"
                AC_MSG_RESULT($rubyldflags)
            fi
            AM_CONDITIONAL([BUILD_RUBY_API], [true])
            AC_DEFINE([HAVE_RUBY], [1], [Ruby support])
        else
            if test "x$enable_ruby_plugins" = xyes; then
                AC_MSG_ERROR([Ruby not found, cannot enable Ruby plugins.])
            else
                AM_CONDITIONAL([BUILD_RUBY_API], [false])
                AC_MSG_NOTICE([Ruby development package not found, Ruby plugin support disabled.])
            fi
        fi
    else
        if test "x$enable_ruby_plugins" = xyes; then
            AC_MSG_ERROR([Ruby not found, cannot enable Ruby plugins.])
        else
            AM_CONDITIONAL([BUILD_RUBY_API], [false])
            AC_MSG_NOTICE([Ruby not installed, Ruby plugin support disabled.])
        fi
    fi
else
    AM_CONDITIONAL([BUILD_RUBY_API], [false])
fi

# python
if test "x$enable_plugins" = xno; then
    AM_CONDITIONAL([BUILD_PYTHON_API], [false])
elif test "x$enable_python_plugins" != xno; then
    AC_CHECK_PROG(PYTHON_CONFIG_EXISTS, python-config, yes, no)
    if test "$PYTHON_CONFIG_EXISTS" == "yes"; then
        AX_PYTHON_DEVEL
        AM_CONDITIONAL([BUILD_PYTHON_API], [true])
        AC_DEFINE([HAVE_PYTHON], [1], [Python support])
    else
        if test "x$enable_python_plugins" = xyes; then
            AC_MSG_ERROR([Python not found, cannot enable Python plugins.])
        else
            AM_CONDITIONAL([BUILD_PYTHON_API], [false])
            AC_MSG_NOTICE([Python development package not found, Python plugin support disabled.])
        fi
    fi
else
    AM_CONDITIONAL([BUILD_PYTHON_API], [false])
fi

# lua
if test "x$enable_plugins" = xno; then
    AM_CONDITIONAL([BUILD_LUA_API], [false])
elif test "x$enable_lua_plugins" != xno; then
    # TODO get the following macros working
    AX_PROG_LUA
    AX_LUA_HEADERS
    AX_LUA_LIBS
    AM_CONDITIONAL([BUILD_LUA_API], [true])
    AC_DEFINE([HAVE_LUA], [1], [Lua support])
else
    AM_CONDITIONAL([BUILD_LUA_API], [false])
fi

# c
LT_INIT
if test "x$enable_plugins" = xno; then
    AM_CONDITIONAL([BUILD_C_API], [false])
elif test "x$enable_c_plugins" != xno; then
    AC_CHECK_LIB([dl], [main],
        [AM_CONDITIONAL([BUILD_C_API], [true]) LIBS="$LIBS -ldl" AC_DEFINE([HAVE_C], [1], [C support])],
        [AS_IF(
            [test "x$enable_c_plugins" = xyes],
                [AC_MSG_ERROR([dl library needed to run C plugins])],
            [AM_CONDITIONAL([BUILD_C_API], [false])])
        ])
else
    AM_CONDITIONAL([BUILD_C_API], [false])
fi

# Use the CFLAGS and libs provided by libstrophe.pc if libstrophe has pkg-config support.
PKG_CHECK_MODULES([libstrophe], [libstrophe], [LIBS="$libstrophe_LIBS $LIBS"],
        [
	 if test "x$PLATFORM" != xfreebsd; then
		AC_CHECK_LIB([resolv], [res_query], [],
		    [AC_CHECK_LIB([resolv], [__res_query], [],
			[AC_MSG_ERROR([libresolv not found; libresolv required for profanity])])])
	 else
		AC_MSG_NOTICE([skipping libresolv checks for freebsd])
	 fi

         ### Check for libstrophe dependencies
         ### Select first existing xml library among expat and libxml2
         PARSER=""
         PARSER_LIBS=""
         PARSER_CFLAGS=""
         AS_IF([test "x$with_libxml2" != xyes],
             [PKG_CHECK_MODULES([expat], [expat],
                 [PARSER_CFLAGS="$expat_CFLAGS"; PARSER_LIBS="$expat_LIBS"; PARSER="expat"],
                 [AC_CHECK_LIB([expat], [XML_ParserCreate],
                     [PARSER_LIBS="-lexpat"; PARSER="expat"],
                     AS_IF([test "x$with_libxml2" = xno],
                         [AC_MSG_ERROR([expat is required but does not exist])]))])
             ])

         AS_IF([test "x$PARSER" = x -a "x$with_libxml2" != xno],
             [PKG_CHECK_MODULES([libxml2], [libxml-2.0],
                 [PARSER_CFLAGS="$libxml2_CFLAGS"; PARSER_LIBS="$libxml2_LIBS"; PARSER="libxml2"],
                 AS_IF([test "x$with_libxml2" = xyes],
                     [AC_MSG_ERROR([libxml2 is required but does not exist])]))
             ])

         AS_IF([test "x$PARSER" = x],
             [AC_MSG_ERROR([either expat or libxml2 is required for profanity])])
         AM_CPPFLAGS="$AM_CPPFLAGS $PARSER_CFLAGS"
         LIBS="$PARSER_LIBS $LIBS"

         PKG_CHECK_MODULES([openssl], [openssl], [],
                 [AC_CHECK_HEADER(openssl/ssl.h, [openssl_LIBS="-lssl -lcrypto"], [AC_MSG_ERROR([openssl not found; openssl required for profanity])])])
         AM_CPPFLAGS="$AM_CPPFLAGS $openssl_CFLAGS"
         LIBS="$openssl_LIBS $LIBS"

         CFLAGS_RESTORE="$CFLAGS"
         CFLAGS="$CFLAGS $AM_CPPFLAGS"
         AC_CHECK_LIB([strophe], [xmpp_ctx_new], [LIBS="-lstrophe $LIBS"; listrophe_CFLAGS=""],
                [AC_MSG_ERROR([libstrophe linked with $PARSER is required for profanity])])
         CFLAGS="$CFLAGS_RESTORE"
        ])
CFLAGS="$CFLAGS $libstrophe_CFLAGS"

### Check for ncurses library
PKG_CHECK_MODULES([ncursesw], [ncursesw],
    [NCURSES_CFLAGS="$ncursesw_CFLAGS"; NCURSES_LIBS="$ncursesw_LIBS"; NCURSES="ncursesw"],
    [PKG_CHECK_MODULES([ncurses], [ncurses],
        [NCURSES_CFLAGS="$ncurses_CFLAGS"; NCURSES_LIBS="$ncurses_LIBS"; NCURSES="ncurses"],
        [AC_CHECK_LIB([ncursesw], [main], [],
            [AC_CHECK_LIB([ncurses], [main], [],
            	[AC_MSG_ERROR([ncurses is required for profanity])])])])])
AM_CPPFLAGS="$AM_CPPFLAGS $NCURSES_CFLAGS"
LIBS="$NCURSES_LIBS $LIBS"
AS_IF([test "x$PLATFORM" = xosx], [LIBS="-lncurses $LIBS"])

### Check wide characters support in ncurses library
CFLAGS_RESTORE="$CFLAGS"
CFLAGS="$CFLAGS $NCURSES_CFLAGS"
AC_CACHE_CHECK([for wget_wch support in $NCURSES], ncurses_cv_wget_wch,
   [AC_LINK_IFELSE([AC_LANG_SOURCE([
       void wget_wch(void);
       int main() {
           wget_wch();
           return 0;
       }
       ])],
       [ncurses_cv_wget_wch=yes],
       [ncurses_cv_wget_wch=no])
   ])
CFLAGS="$CFLAGS_RESTORE"

AS_IF([test "x$ncurses_cv_wget_wch" != xyes],
    [AC_MSG_ERROR([ncurses does not support wide characters])])

### Check for other profanity dependencies
PKG_CHECK_MODULES([glib], [glib-2.0 >= 2.26], [],
    [AC_MSG_ERROR([glib 2.26 or higher is required for profanity])])
PKG_CHECK_MODULES([curl], [libcurl], [],
    [AC_MSG_ERROR([libcurl is required for profanity])])

AS_IF([test "x$PLATFORM" != xosx],
    [AC_CHECK_LIB([readline], [main], [],
        [AC_MSG_ERROR([libreadline is required for profanity])])],
    [AC_CHECK_FILE([/usr/local/opt/readline/lib],
        [LIBS="-lreadline $LIBS"
            AM_CPPFLAGS="-I/usr/local/opt/readline/include $AM_CPPFLAGS"
            AM_LDFLAGS="-L/usr/local/opt/readline/lib $AM_LDFLAGS"
            AC_SUBST(AM_LDFLAGS)],
        [AC_MSG_ERROR([libreadline is required for profanity])])])

AS_IF([test "x$PLATFORM" = xosx], [LIBS="-lcurl $LIBS"])

### Check for desktop notification support
### Linux requires libnotify
### Windows uses native OS calls
### OSX requires terminal-notifier

AS_IF([test "x$PLATFORM" = xosx],
        [AS_IF([test "x$enable_notifications" != xno],
            [NOTIFIER_PATH="no"
            AC_PATH_PROG(NOTIFIER_PATH, terminal-notifier, no)
            AS_IF([test "x$NOTIFIER_PATH" = xno],
                [AS_IF([test "x$enable_notifications" = xyes],
                    [AC_MSG_ERROR([terminal-notifier not found, required for desktop notifications.])],
                    [AC_MSG_NOTICE([Desktop notifications not supported.])])],
                [AC_DEFINE([HAVE_OSXNOTIFY], [1], [terminal notifier])])])],
    [test "x$PLATFORM" = xnix],
        [AS_IF([test "x$enable_notifications" != xno],
            [PKG_CHECK_MODULES([libnotify], [libnotify],
                [AC_DEFINE([HAVE_LIBNOTIFY], [1], [libnotify module])],
                [AS_IF([test "x$enable_notifications" = xyes],
                    [AC_MSG_ERROR([libnotify is required but does not exist])],
                    [AC_MSG_NOTICE([libnotify support will be disabled])])])])])

# TODO: rewrite this
if test "x$with_xscreensaver" = xyes; then
    AC_CHECK_LIB([Xss], [main], [],
        [AC_MSG_ERROR([libXss is required for x autoaway support])])
    AC_CHECK_LIB([X11], [main], [],
        [AC_MSG_ERROR([libX11 is required for x autoaway support])])
elif test "x$with_xscreensaver" = x; then
    AC_CHECK_LIB([Xss], [main], [],
        [AC_MSG_NOTICE([libXss not found, falling back to profanity auto-away])])
    AC_CHECK_LIB([X11], [main], [],
        [AC_MSG_NOTICE([libX11 not found, falling back to profanity auto-away])])
fi

AM_CONDITIONAL([BUILD_OTR], [false])
AM_CONDITIONAL([BUILD_OTR3], [false])
AM_CONDITIONAL([BUILD_OTR4], [false])
if test "x$enable_otr" = xyes; then
    AC_SEARCH_LIBS([otrl_init], [otr],
        [AC_COMPILE_IFELSE(
            [AC_LANG_PROGRAM([[
            #include <libotr/version.h>
            ]],[[
            #if OTRL_VERSION_MAJOR == 4
            // OK
            #else
            # assume version 3
            #endif
            ]])],
            [AM_CONDITIONAL([BUILD_OTR], [true]) AM_CONDITIONAL([BUILD_OTR4], [true]) AC_DEFINE([HAVE_LIBOTR], [1], [Have libotr])],
            [AM_CONDITIONAL([BUILD_OTR], [true]) AM_CONDITIONAL([BUILD_OTR3], [true]) AC_DEFINE([HAVE_LIBOTR], [1], [Have libotr])])],
        [AC_MSG_ERROR([libotr is required for otr encryption support])])
elif test "x$enable_otr" = x; then
    AC_SEARCH_LIBS([otrl_init], [otr],
        [AC_COMPILE_IFELSE(
            [AC_LANG_PROGRAM([[
            #include <libotr/version.h>
            ]],[[
            #if OTRL_VERSION_MAJOR == 4
            // OK
            #else
            # assume version 3
            #endif
            ]])],
            [AM_CONDITIONAL([BUILD_OTR], [true]) AM_CONDITIONAL([BUILD_OTR4], [true]) AC_DEFINE([HAVE_LIBOTR], [1], [Have libotr])],
            [AM_CONDITIONAL([BUILD_OTR], [true]) AM_CONDITIONAL([BUILD_OTR3], [true]) AC_DEFINE([HAVE_LIBOTR], [1], [Have libotr])])],
        [AC_MSG_NOTICE([libotr not found, otr encryption support not enabled])])
fi

AS_IF([test "x$with_themes" = xno],
    [THEMES_INSTALL="false"],
    [THEMES_INSTALL="true"])
AS_IF([test "x$with_themes" = xno -o "x$with_themes" = xyes -o "x$with_themes" = x],
    [THEMES_PATH='${pkgdatadir}/themes'],
    [THEMES_PATH="$with_themes"])
AC_SUBST(THEMES_PATH)
AM_CONDITIONAL([THEMES_INSTALL], "$THEMES_INSTALL")

### cmocka is required only for tests, profanity shouldn't be linked with it
### TODO: pass cmocka_CFLAGS and cmocka_LIBS to Makefile.am
PKG_CHECK_MODULES([cmocka], [cmocka], [],
    [AC_MSG_NOTICE([cmocka is not found, will not be able to run tests])])

### Check for ncursesw/ncurses.h first, Arch linux uses ncurses.h for ncursesw
AC_CHECK_HEADERS([ncursesw/ncurses.h], [], [])
AC_CHECK_HEADERS([ncurses.h], [], [])

### Default parameters
AM_CFLAGS="-Wall -Wno-deprecated-declarations"
AS_IF([test "x$PACKAGE_STATUS" = xdevelopment],
    [AM_CFLAGS="$AM_CFLAGS -Wunused -Werror"])
AM_LDFLAGS="$AM_LDFLAGS $PYTHON_LDFLAGS $RUBY_LDFLAGS $LUA_LIB -export-dynamic"
AM_CPPFLAGS="$AM_CPPFLAGS $glib_CFLAGS $curl_CFLAGS $libnotify_CFLAGS $PYTHON_CPPFLAGS $RUBY_CFLAGS $LUA_INCLUDE"
AM_CPPFLAGS="$AM_CPPFLAGS -DTHEMES_PATH=\"\\\"$THEMES_PATH\\\"\""
LIBS="$glib_LIBS $curl_LIBS $libnotify_LIBS $RUBY_LIBS $LIBS"

AC_SUBST(AM_LDFLAGS)
AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_CPPFLAGS)

### Checks for library functions.
AC_CHECK_FUNCS([atexit memset strdup strstr])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo ""
echo "PLATFORM       : $host_os"
echo "PACKAGE_STATUS : $PACKAGE_STATUS"
echo "AM_CFLAGS      : $AM_CFLAGS"
echo "AM_CPPFLAGS    : $AM_CPPFLAGS"
echo "AM_LDFLAGS     : $AM_LDFLAGS"
echo "LIBS           : $LIBS"
echo "XML Parser     : $PARSER"
echo "Install themes : $THEMES_INSTALL"
echo "Themes path    : $THEMES_PATH"
echo ""
echo "Now you can run \`make' to build profanity"
