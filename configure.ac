AC_INIT(gmerlin-avdecoder, 2.0.0)
AC_CONFIG_SRCDIR([include/avdec.h])
AM_CONFIG_HEADER(include/config.h)
AM_INIT_AUTOMAKE(1.8.5)


BGAV_VERSION=$VERSION

BGAV_VERSION_MAJOR=`echo $VERSION | cut -d . -f 1`
BGAV_VERSION_MINOR=`echo $VERSION | cut -d . -f 2`
BGAV_VERSION_MICRO=`echo $VERSION | cut -d . -f 3 | cut -d p -f 1`

LTVERSION_CURRENT="2"
LTVERSION_REVISION="0"
LTVERSION_AGE="0"

AC_SUBST(BGAV_VERSION)
AC_SUBST(BGAV_VERSION_MAJOR)
AC_SUBST(BGAV_VERSION_MINOR)
AC_SUBST(BGAV_VERSION_MICRO)

AC_SUBST(LTVERSION_CURRENT)
AC_SUBST(LTVERSION_REVISION)
AC_SUBST(LTVERSION_AGE)

OLD_CFLAGS=$CFLAGS
XTRA_LDFLAGS=$LDFLAGS
AC_SUBST([XTRA_LDFLAGS])

AC_LANG(C)

AC_GNU_SOURCE

AC_DISABLE_STATIC
AM_PROG_AS
AC_PROG_LIBTOOL

CFLAGS=$OLD_CFLAGS

dnl
dnl General stuff
dnl

AC_CHECK_HEADERS(byteswap.h)

AC_C_BIGENDIAN(,,AC_MSG_ERROR("Cannot detect endianess"))

AC_SYS_LARGEFILE
AC_FUNC_FSEEKO
AC_CHECK_FUNCS(ftello vasprintf isatty)

AC_CHECK_DECLS([MSG_NOSIGNAL, SO_NOSIGPIPE],,,
               [#include <sys/types.h>
	       #include <sys/socket.h>])

dnl
dnl Semaphores
dnl

GMERLIN_CHECK_SEMAPHORES

dnl
dnl Gettext
dnl

AM_GNU_GETTEXT(external)

dnl Check for mingw

case "$build" in
	*cygwin*)
	CYGWIN_LDFLAGS="-no-undefined -mwindows"
	AC_MSG_WARN([build is $build])
	;;
	*)
	CYGWIN_LDFLAGS=
	;;
esac

case "$host" in
	*-*-mingw*|*cygwin*)
	os_win32=yes		
	AC_MSG_WARN([host is $host])
	;;
	*)
	os_win32=no
	;;
esac
AM_CONDITIONAL(OS_WIN32, [test "$os_win32" = "yes"])

if test "$os_win32" = "yes"; then
	dnl LIBS="-lws2_32   -lwsnmp32  -lwsock32  -lwst -lwinstrm -lnetapi32 -lws2_32 -lwsock32 -lwinmm -lwinstrm -lpthreadGC2 -lltdl -lregex -lz -lkernel32 -luser32 -lgdi32 $LIBS"
	LIBS=" -lpthreadGC2  -lregex -lz -lws2_32 -lintl $LIBS"
	dnl it appears that -no-undefined is necessary in mingw in order to build dll
	dnl maybe we should check here if the user only wants static libs
        LDFLAGS="$CYGWIN_LDFLAGS -mms-bitfields $LDFLAGS"
	AC_CHECK_LIB(pthreadGC2, pthread_mutex_lock, , AC_MSG_ERROR([libpthread not found get it from ftp://sourceware.org/pub/pthreads-win32/]))
else
	AC_CHECK_LIB(pthread, pthread_mutex_lock, , AC_MSG_ERROR([libpthread not found but you are not using windows.  You should really have this.]))
fi



dnl
dnl Doxygen
dnl

test_doxygen="true"
have_doxygen="false"

AC_ARG_WITH(doxygen,
    AC_HELP_STRING([--without-doxygen],
    [disable documentation generation]),
    test_doxygen="false")

if test "x$test_doxygen" = "xtrue"; then
  AC_CHECK_PROG(DOXYGEN, doxygen, "doxygen")
  if test "x$DOXYGEN" != "x"; then
    have_doxygen="true"
  fi
fi

AM_CONDITIONAL(HAVE_DOXYGEN, test "x$have_doxygen" = "xtrue")

dnl
dnl Linker flags
dnl 

GMERLIN_CHECK_LDFLAGS

dnl
dnl Check for Libraries
dnl

GAVL_REQUIRED="2.0.0"

PKG_CHECK_MODULES(GAVL, gavl >= $GAVL_REQUIRED, , AC_MSG_ERROR("gavl not found"))
AC_SUBST(GAVL_REQUIRED)

ICONV_LIBS=
AC_CHECK_LIB(iconv, libiconv_close, ICONV_LIBS="-liconv")
AC_SUBST(ICONV_LIBS)

if test "x$os_win32" = "xyes"; then
AC_CHECK_LIB(regex, regcomp, , AC_MSG_ERROR([libregex not found.]))
AC_CHECK_LIB(gettextlib, rpl_open, , AC_MSG_ERROR([libgettextlib not found get it from http://www.gnu.org/software/gettext/]))
AC_CHECK_LIB(intl, libintl_sprintf, , AC_MSG_ERROR([libintl not found get it from http://www.gnu.org/software/gettext/]))
fi

AC_CHECK_HEADER(dlfcn.h, , AC_MSG_ERROR([dlfcn.h not found]))
AC_CHECK_LIB(z, uncompress, , AC_MSG_ERROR([libz not found get it from http://zlib.net]))


dnl
dnl Library functions
dnl

AC_CHECK_FUNCS([poll getaddrinfo inet_aton closesocket])

dnl
dnl Optional Libraries
dnl

dnl
dnl gmerlin
dnl 

have_gmerlin="false"

GMERLIN_REQUIRED="2.0.0"

AC_ARG_ENABLE(gmerlin,
[AC_HELP_STRING([--disable-gmerlin],[Disable gmerlin plugins (default: autodetect)])],
[case "${enableval}" in
   yes) test_gmerlin=true ;;
   no)  test_gmerlin=false ;;
esac],[test_gmerlin=true])

if test x$test_gmerlin = xtrue; then

PKG_CHECK_MODULES(GMERLIN, gmerlin >= $GMERLIN_REQUIRED, have_gmerlin="true",
have_gmerlin="false")

gmerlin_plugindir='$(libdir)/gmerlin/plugins'

dnl LDFLAGS for plugins
GMERLIN_PLUGIN_LDFLAGS="-export-symbols "`pkg-config --variable=prefix gmerlin`"/share/gmerlin/plugin.sym ${XTRA_LDFLAGS}"

fi

AC_SUBST(gmerlin_plugindir)
AC_SUBST(GMERLIN_REQUIRED)
AC_SUBST(GMERLIN_PLUGIN_LDFLAGS)

AM_CONDITIONAL(HAVE_GMERLIN, test x$have_gmerlin = xtrue)

dnl
dnl AVCodec
dnl

GMERLIN_CHECK_AVCODEC

dnl
dnl AVFormat
dnl

GMERLIN_CHECK_AVFORMAT

dnl
dnl OpenGL
dnl

GMERLIN_CHECK_OPENGL

dnl
dnl V4L2
dnl

GMERLIN_CHECK_V4L2



dnl
dnl X11 (need to check before vdpau)
dnl

have_x="false"

X_CFLAGS=""
X_LIBS=""


AC_PATH_X

if test x$no_x != xyes; then
  if test "x$x_includes" != "x"; then
    X_CFLAGS="-I$x_includes"
  elif test -d /usr/X11R6/include; then 
    X_CFLAGS="-I/usr/X11R6/include"
  else
    X_CFLAGS=""
  fi

  if test "x$x_libraries" != "x"; then
    X_LIBS="-L$x_libraries -lX11"
  else
    X_LIBS="-lX11"
  fi
  have_x="true"
else
  PKG_CHECK_MODULES(X, x11 >= 1.0.0, have_x=true, have_x=false)
fi

if test x$have_x = xtrue; then
  X_LIBS="$X_LIBS -lXext"
fi


dnl
dnl X11
dnl

GMERLIN_CHECK_X11

dnl
dnl libva
dnl

GMERLIN_CHECK_LIBVA

dnl
dnl Check for theora
dnl

GMERLIN_CHECK_THEORADEC

dnl
dnl Check for schroedinger
dnl

GMERLIN_CHECK_SCHROEDINGER

dnl
dnl Check for speex
dnl

GMERLIN_CHECK_SPEEX

dnl
dnl Ogg
dnl 

GMERLIN_CHECK_OGG

dnl
dnl Vorbis
dnl 

GMERLIN_CHECK_VORBIS

dnl
dnl Opus
dnl

GMERLIN_CHECK_OPUS


dnl
dnl libtiff
dnl

GMERLIN_CHECK_LIBTIFF

dnl
dnl openjpeg
dnl

dnl GMERLIN_CHECK_OPENJPEG

dnl
dnl libsmbclient
dnl

GMERLIN_CHECK_SAMBA

dnl
dnl PNG
dnl 

GMERLIN_CHECK_LIBPNG


dnl
dnl DVDREAD
dnl

GMERLIN_CHECK_DVDREAD

dnl
dnl FLAC
dnl

GMERLIN_CHECK_FLAC

dnl
dnl Musepack
dnl

GMERLIN_CHECK_MUSEPACK

dnl
dnl MAD
dnl

GMERLIN_CHECK_MAD

dnl
dnl liba52
dnl

GMERLIN_CHECK_LIBA52

dnl
dnl libdca
dnl

GMERLIN_CHECK_DCA

dnl
dnl CDrom support
dnl

GMERLIN_CHECK_CDIO

dnl
dnl libudf support
dnl

GMERLIN_CHECK_LIBUDF

dnl
dnl Linux DVB support
dnl 

GMERLIN_CHECK_LINUXDVB

dnl
dnl Option for disabling libgsm
dnl

AH_TEMPLATE([HAVE_LIBGSM], [ libgsm enabled ])
have_libgsm="true"

AC_ARG_WITH(GSM610,
    AC_HELP_STRING([--without-GSM610],
    [disable libgsm]),
    have_libgsm="false", have_libgsm="true")

if test "x$have_libgsm" = "xtrue"; then
AC_DEFINE([HAVE_LIBGSM])
fi

AM_CONDITIONAL(HAVE_LIBGSM, test "x$have_libgsm" = "xtrue")

dnl
dnl Build optimization flags
dnl

LQT_OPT_CFLAGS(["-O3 -ffast-math"])
AC_SUBST(OPT_CFLAGS)

dnl
dnl Common CFLAGS
dnl
LIBBGAV_CFLAGS=""

CFLAGS="$CFLAGS -D_REENTRANT -D_FILE_OFFSET_BITS=64 $GAVL_CFLAGS"

LQT_TRY_CFLAGS(-Wall, CFLAGS="$CFLAGS -Wall", )
LQT_TRY_CFLAGS(-Wmissing-declarations, CFLAGS="$CFLAGS -Wmissing-declarations", )
LQT_TRY_CFLAGS(-Wdeclaration-after-statement, CFLAGS="$CFLAGS -Wdeclaration-after-statement")
LQT_TRY_CFLAGS(-fvisibility=hidden,
               LIBBGAV_CFLAGS="$LIBBGAV_CFLAGS -fvisibility=hidden", )


dnl The following is necessary so the win32 dll loader also gets compiled
dnl with -g

if test x$LQT_DEBUG = xtrue; then
  CFLAGS="-g $CFLAGS"
fi

LIBBGAV_CFLAGS="-D__BGAV__ $LIBBGAV_CFLAGS"
AC_SUBST(LIBBGAV_CFLAGS)

dnl
dnl Common libs
dnl 

LIBS="$LIBS $GAVL_LIBS"

if test "x$LQT_PROFILE" = "xtrue"; then
LIBS="$LIBS $GAVL_LIBS -pg"
fi

dnl Absolute src path for doxygen

TOP_SRCDIR=`cd ${srcdir} && pwd` 
AC_SUBST(TOP_SRCDIR)

dnl
dnl Output
dnl 

AC_CONFIG_FILES([Makefile 
doc/Makefile \
doc/Doxyfile \
po/Makefile.in \
utils/Makefile \
gmerlin_avdec.pc \
include/Makefile \
include/bgav_version.h \
lib/Makefile \
lib/GSM610/Makefile \
m4/Makefile \
plugins/Makefile \
tests/Makefile ])

AC_OUTPUT

echo ""
echo "=== Libraries: ================================="

echo -n "gmerlin:                "
if test "x$have_gmerlin" = "xtrue"; then
echo "Found (CFLAGS=$GMERLIN_CFLAGS LIBS=$GMERLIN_LIBS)"
elif test "x$test_gmerlin" = "xfalse"; then
echo "Disabled"
elif test "x$gmerlin_wrong_prefix" = "xtrue"; then
echo "Wrong installation prefix"
else
echo "Missing (Go to http://gmerlin.sourceforge.net)"
fi

echo -n "libavcodec (ffmpeg):    "
if test "x$have_avcodec" = "xtrue"; then
echo "Found (CFLAGS=$AVCODEC_CFLAGS LIBS=$AVCODEC_LIBS)"
elif test "x$test_avcodec" = "xfalse"; then
echo "Disabled"
elif test "x$avcodec_swscale_missing" = "xtrue"; then
echo "Needs libswscale (Go to http://www.ffmpeg.org/)"
else
echo "Missing (Go to http://www.ffmpeg.org/)"
fi

echo -n "libavformat (ffmpeg):   "
if test "x$have_avformat" = "xtrue"; then
echo "Found (CFLAGS=$AVFORMAT_CFLAGS LIBS=$AVFORMAT_LIBS)"
elif test "x$test_avformat" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://www.ffmpeg.org/)"
fi

echo -n "flac:                   "
if test "x$have_flac" = "xtrue"; then
echo "Found (CFLAGS=$FLAC_CFLAGS LIBS=$FLAC_LIBS)"
elif test "x$test_flac" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://flac.sourceforge.net/)"
fi

echo -n "vorbis:                 "
if test "x$have_vorbis" = "xtrue"; then
echo "Found (CFLAGS=$VORBIS_CFLAGS LIBS=$VORBIS_LIBS)"
elif test "x$test_vorbis" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://www.vorbis.com/)"
fi

echo -n "theoradec:              "
if test "x$have_theoradec" = "xtrue"; then
echo "Found (CFLAGS=$THEORADEC_CFLAGS LIBS=$THEORADEC_LIBS)"
elif test "x$test_theoradec" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://www.theora.org/)"
fi

echo -n "schroedinger:           "
if test "x$have_schroedinger" = "xtrue"; then
echo "Found (CFLAGS=$SCHROEDINGER_CFLAGS LIBS=$SCHROEDINGER_LIBS)"
elif test "x$test_schroedinger" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://www.diracvideo.org/)"
fi

echo -n "speex:                  "
if test "x$have_speex" = "xtrue"; then
echo "Found (CFLAGS=$SPEEX_CFLAGS LIBS=$SPEEX_LIBS)"
elif test "x$test_speex" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://www.speex.org/)"
fi

echo -n "libpng:                 "
if test "x$have_libpng" = "xtrue"; then
echo "Found (CFLAGS=$PNG_CFLAGS LIBS=$PNG_LIBS)"
elif test "x$test_libpng" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://www.libpng.org/pub/png/libpng.html)"
fi

echo -n "libtiff:                "
if test "x$have_libtiff" = "xtrue"; then
echo "Found (CFLAGS=$TIFF_CFLAGS LIBS=$TIFF_LIBS)"
elif test "x$test_libtiff" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://www.remotesensing.org/libtiff/)"
fi

echo -n "libopenjpeg:            "
if test "x$have_openjpeg" = "xtrue"; then
echo "Found (CFLAGS=$OPENJPEG_CFLAGS LIBS=$OPENJPEG_LIBS)"
elif test "x$test_openjpeg" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://www.openjpeg.org)"
fi

echo -n "libmad 0.15.x:          "
if test "x$have_mad" = "xtrue"; then
echo "Found (CFLAGS=$MAD_CFLAGS LIBS=$MAD_LIBS)"
elif test "x$test_mad" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://mad.sourceforge.net/)"
fi

echo -n "liba52 (a52dec):        "
if test "x$have_liba52" = "xtrue"; then
echo "Found (CFLAGS=$LIBA52_CFLAGS LIBS=$LIBA52_LIBS)"
elif test "x$test_liba52" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://liba52.sourceforge.net/)"
fi

echo -n "libdca:                 "
if test "x$have_dca" = "xtrue"; then
echo "Found (CFLAGS=$DCA_CFLAGS LIBS=$DCA_LIBS)"
elif test "x$test_cda" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://www.videolan.org/developers/libdca.html)"
fi

echo -n "libmpcdec:              "
if test "x$have_musepack" = "xtrue"; then
echo "Found (CFLAGS=$MUSEPACK_CFLAGS LIBS=$MUSEPACK_LIBS)"
elif test "x$test_musepack" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://www.musepack.net/)"
fi

echo -n "libcdio:                "
if test "x$have_cdio" = "xtrue"; then
echo "Found (CFLAGS=$CDIO_CFLAGS LIBS=$CDIO_LIBS)"
elif test "x$test_cdio" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://www.gnu.org/software/libcdio/)"
fi

echo -n "libudf:                 "
if test "x$have_libudf" = "xtrue"; then
echo "Found (CFLAGS=$CDIO_CFLAGS LIBS=$LIBUDF_LIBS)"
elif test "x$test_cdio" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://www.gnu.org/software/libcdio/)"
fi


echo -n "samba:                  "
if test "x$have_libsmbclient" = "xtrue"; then
echo "Found (CFLAGS=$SAMBA_CFLAGS LIBS=$SAMBA_LIBS)"
elif test "x$test_libsmbclient" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://www.samba.org)"
fi

echo -n "libdvdread:             "
if test "x$have_dvdread" = "xtrue"; then
echo "Found (CFLAGS=$DVDREAD_CFLAGS LIBS=$DVDREAD_LIBS)"
elif test "x$test_dvdread" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://www.dtek.chalmers.se/groups/dvd/downloads.shtml)"
fi

echo -n "libopus:                "
if test "x$have_opus" = "xtrue"; then
echo "Found (CFLAGS=$OPUS_CFLAGS LIBS=$OPUS_LIBS)"
elif test "x$test_opus" = "xfalse"; then
echo "Disabled"
else
echo "Missing (Go to http://www.opus-codec.org)"
fi


echo ""
echo "=== Other features: ============================"

echo -n "Linux DVB Support:      "
if test "x$have_linuxdvb" = "xtrue"; then
echo "Enabled"
else
echo "Disabled (Unsupported Architecture or kernel headers missing)"
fi

echo
echo "If you installed a library but it was not detected, check the file INSTALL"
echo "for troubleshooting tips. Also note that if the configure script reaches"
echo "this point, all missing packages are optional so compilation should succeed"
echo "anyway."


