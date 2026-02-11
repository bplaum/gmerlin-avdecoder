dnl
dnl Standardized linker flags:
dnl We use --as-needed for executables and
dnl --no-undefined for libraries
dnl

AC_DEFUN([GMERLIN_CHECK_LDFLAGS],[

GMERLIN_LIB_LDFLAGS=""
GMERLIN_EXE_LDFLAGS=""

AC_MSG_CHECKING(if linker supports --no-undefined)
OLD_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS -Wl,--no-undefined"

AC_LINK_IFELSE([AC_LANG_SOURCE([[int main() { return 0; } ]])],
            [GMERLIN_LIB_LDFLAGS="-Wl,--no-undefined $GMERLIN_LIB_LDFLAGS"; AC_MSG_RESULT(Supported)],
            [AC_MSG_RESULT(Unsupported)])
LDFLAGS=$OLD_LDFLAGS

AC_MSG_CHECKING(if linker supports --as-needed)
OLD_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS -Wl,--as-needed"
AC_LINK_IFELSE([AC_LANG_SOURCE([[int main() { return 0; }]])],
            [GMERLIN_EXE_LDFLAGS="-Wl,--as-needed $GMERLIN_EXE_LDFLAGS"; AC_MSG_RESULT(Supported)],
            [AC_MSG_RESULT(Unsupported)])
LDFLAGS=$OLD_LDFLAGS

AC_SUBST(GMERLIN_LIB_LDFLAGS)
AC_SUBST(GMERLIN_EXE_LDFLAGS)

])

dnl
dnl Check for theora decoder
dnl

AC_DEFUN([GMERLIN_CHECK_THEORADEC],[

AH_TEMPLATE([HAVE_THEORADEC],
            [Do we have theora decoder installed?])

have_theora="false"

THEORADEC_REQUIRED="1.0.0"

AC_ARG_ENABLE(theoradec,
[AS_HELP_STRING([--disable-theoradec],[Disable theoradec (default: autodetect)])],
[case "${enableval}" in
   yes) test_theoradec=true ;;
   no)  test_theoradec=false ;;
esac],[test_theoradec=true])

if test x$test_theoradec = xtrue; then

PKG_CHECK_MODULES(THEORADEC, theoradec, have_theoradec="true", have_theoradec="false")
fi

AC_SUBST(THEORADEC_REQUIRED)
AC_SUBST(THEORADEC_LIBS)
AC_SUBST(THEORADEC_CFLAGS)

AM_CONDITIONAL(HAVE_THEORADEC, test x$have_theoradec = xtrue)

if test "x$have_theoradec" = "xtrue"; then
AC_DEFINE([HAVE_THEORADEC])
fi

])


dnl
dnl Check for libavcodec
dnl

AC_DEFUN([GMERLIN_CHECK_AVCODEC],[

AH_TEMPLATE([HAVE_AVCODEC],
            [Do we have libavcodec installed?])

have_libavcodec="false"

AVCODEC_REQUIRED="58.54.100"

AC_ARG_ENABLE(avcodec,
[AS_HELP_STRING([--disable-avcodec],[Disable libavcodec (default: autodetect)])],
[case "${enableval}" in
   yes) test_avcodec=true ;;
   no)  test_avcodec=false ;;
esac],[test_avcodec=true])

if test x$test_avcodec = xtrue; then

PKG_CHECK_MODULES(AVCODEC, libavcodec, have_avcodec="true", have_avcodec="false")
fi

AC_SUBST(AVCODEC_REQUIRED)
AC_SUBST(AVCODEC_LIBS)
AC_SUBST(AVCODEC_CFLAGS)

AM_CONDITIONAL(HAVE_AVCODEC, test x$have_avcodec = xtrue)

if test "x$have_avcodec" = "xtrue"; then
AC_DEFINE([HAVE_AVCODEC])
fi

])

AC_DEFUN([GMERLIN_CHECK_AVFORMAT],[

AH_TEMPLATE([HAVE_AVFORMAT],
            [Do we have libavformat installed?])

have_libavformat="false"

AVFORMAT_REQUIRED="58.29.100"

AC_ARG_ENABLE(avformat,
[AS_HELP_STRING([--disable-avformat],[Disable libavformat (default: autodetect)])],
[case "${enableval}" in
   yes) test_avformat=true ;;
   no)  test_avformat=false ;;
esac],[test_avformat=true])

if test x$test_avformat = xtrue; then

PKG_CHECK_MODULES(AVFORMAT, libavformat, have_avformat="true", have_avformat="false")
fi

AC_SUBST(AVFORMAT_REQUIRED)
AC_SUBST(AVFORMAT_LIBS)
AC_SUBST(AVFORMAT_CFLAGS)

AM_CONDITIONAL(HAVE_AVFORMAT, test x$have_avformat = xtrue)

if test "x$have_avformat" = "xtrue"; then
AC_DEFINE([HAVE_AVFORMAT])
fi

])

dnl
dnl Check for speex
dnl

AC_DEFUN([GMERLIN_CHECK_SPEEX],[

AH_TEMPLATE([HAVE_SPEEX],
            [Do we have speex installed?])

have_speex="false"

SPEEX_REQUIRED="1.0.4"

AC_ARG_ENABLE(speex,
[AS_HELP_STRING([--disable-speex],[Disable speex (default: autodetect)])],
[case "${enableval}" in
   yes) test_speex=true ;;
   no)  test_speex=false ;;
esac],[test_speex=true])

if test x$test_speex = xtrue; then

PKG_CHECK_MODULES(SPEEX, speex >= $SPEEX_REQUIRED, have_speex="true", have_speex="false")

fi

AC_SUBST(SPEEX_REQUIRED)
AC_SUBST(SPEEX_LIBS)
AC_SUBST(SPEEX_CFLAGS)

AM_CONDITIONAL(HAVE_SPEEX, test x$have_speex = xtrue)

if test "x$have_speex" = "xtrue"; then
AC_DEFINE([HAVE_SPEEX])
fi

])

dnl
dnl Check for libdvdnav
dnl

AC_DEFUN([GMERLIN_CHECK_DVDNAV],[

AH_TEMPLATE([HAVE_DVDNAV],
            [Do we have libdvdnav installed?])

have_dvdnav="false"

DVDNAV_REQUIRED="6.0.0"

AC_ARG_ENABLE(dvdnav,
[AS_HELP_STRING([--disable-dvdnav],[Disable libdvdnav (default: autodetect)])],
[case "${enableval}" in
   yes) test_dvdnav=true ;;
   no)  test_dvdnav=false ;;
esac],[test_dvdnav=true])

if test x$test_dvdnav = xtrue; then

PKG_CHECK_MODULES(DVDNAV, dvdnav >= $DVDNAV_REQUIRED, have_dvdnav="true", have_dvdnav="false")

fi

AC_SUBST(DVDNAV_REQUIRED)
AC_SUBST(DVDNAV_LIBS)
AC_SUBST(DVDNAV_CFLAGS)

AM_CONDITIONAL(HAVE_DVDNAV, test x$have_dvdnav = xtrue)

if test "x$have_dvdnav" = "xtrue"; then
AC_DEFINE([HAVE_DVDNAV])
fi

])

dnl
dnl Ogg
dnl 

AC_DEFUN([GMERLIN_CHECK_OGG],[

OGG_REQUIRED="1.0"

have_ogg=false
AH_TEMPLATE([HAVE_OGG], [Ogg libraries are there])

AC_ARG_ENABLE(ogg,
[AS_HELP_STRING([--disable-ogg],[Disable ogg (default: autodetect)])],
[case "${enableval}" in
   yes) test_ogg=true ;;
   no)  test_ogg=false ;;
esac],[test_ogg=true])

if test x$test_ogg = xtrue; then
PKG_CHECK_MODULES(OGG, ogg, have_ogg="true", have_ogg="false")
fi

AC_SUBST(OGG_LIBS)
AC_SUBST(OGG_CFLAGS)

AM_CONDITIONAL(HAVE_OGG, test x$have_ogg = xtrue)
 
if test x$have_ogg = xtrue; then
AC_DEFINE(HAVE_OGG)
fi

AC_SUBST(OGG_REQUIRED)

])



dnl
dnl Vorbis
dnl 

AC_DEFUN([GMERLIN_CHECK_VORBIS],[

VORBIS_REQUIRED="1.0"

have_vorbis=false
AH_TEMPLATE([HAVE_VORBIS], [Vorbis libraries are there])

AC_ARG_ENABLE(vorbis,
[AS_HELP_STRING([--disable-vorbis],[Disable vorbis (default: autodetect)])],
[case "${enableval}" in
   yes) test_vorbis=true ;;
   no)  test_vorbis=false ;;
esac],[test_vorbis=true])

if test x$test_vorbis = xtrue; then
PKG_CHECK_MODULES(VORBIS, vorbis, have_vorbis="true", have_vorbis="false")
fi

AM_CONDITIONAL(HAVE_VORBIS, test x$have_vorbis = xtrue)
 
if test x$have_vorbis = xtrue; then
AC_DEFINE(HAVE_VORBIS)

OLD_CFLAGS=$CFLAGS
OLD_LIBS=$LIBS

CFLAGS="$VORBIS_CFLAGS"
LIBS="$VORBIS_LIBS"

AC_CHECK_FUNCS(vorbis_synthesis_restart)

CFLAGS="$OLD_CFLAGS"
LIBS="$OLD_LIBS"


fi

AC_SUBST(VORBIS_REQUIRED)

])

dnl
dnl libtiff
dnl

AC_DEFUN([GMERLIN_CHECK_LIBTIFF],[

AH_TEMPLATE([HAVE_LIBTIFF], [Enable tiff codec])
 
have_libtiff=false
TIFF_REQUIRED="3.5.0"

AC_ARG_ENABLE(libtiff,
[AS_HELP_STRING([--disable-libtiff],[Disable libtiff (default: autodetect)])],
[case "${enableval}" in
   yes) test_libtiff=true ;;
   no)  test_libtiff=false ;;
esac],[test_libtiff=true])

if test x$test_libtiff = xtrue; then
   
OLD_LIBS=$LIBS

LIBS="$LIBS -ltiff"
   
AC_MSG_CHECKING(for libtiff)
AC_LINK_IFELSE([AC_LANG_SOURCE([[#include <tiffio.h>
				int main()
				{
				TIFF * tiff = (TIFF*)0;
				int i = 0;
				/* We ensure the function is here but never call it */
				if(i)
				  TIFFReadRGBAImage(tiff, 0, 0, (uint32*)0, 0);
				return 0;    
				}]])],
            [have_libtiff=true])
 
case $have_libtiff in
  true) AC_DEFINE(HAVE_LIBTIFF)
        AC_MSG_RESULT(yes)
        TIFF_LIBS=$LIBS;;
  false) AC_MSG_RESULT(no); TIFF_LIBS=""; TIFF_CFLAGS="";;
esac
LIBS=$OLD_LIBS

fi

AC_SUBST(TIFF_CFLAGS)
AC_SUBST(TIFF_LIBS)
AC_SUBST(TIFF_REQUIRED)

AM_CONDITIONAL(HAVE_LIBTIFF, test x$have_libtiff = xtrue)

if test x$have_libtiff = xtrue; then
AC_DEFINE(HAVE_LIBTIFF)
fi

])

dnl
dnl PNG
dnl 

AC_DEFUN([GMERLIN_CHECK_LIBPNG],[

AH_TEMPLATE([HAVE_LIBPNG], [Enable png codec])
 
have_libpng=false
PNG_REQUIRED="1.2.2"

AC_ARG_ENABLE(libpng,
[AS_HELP_STRING([--disable-libpng],[Disable libpng (default: autodetect)])],
[case "${enableval}" in
   yes) test_libpng=true ;;
   no)  test_libpng=false ;;
esac],[test_libpng=true])

if test x$test_libpng = xtrue; then

PKG_CHECK_MODULES(PNG, libpng, have_libpng="true", have_libpng="false")
fi

AC_SUBST(PNG_CFLAGS)
AC_SUBST(PNG_LIBS)
AC_SUBST(PNG_REQUIRED)

AM_CONDITIONAL(HAVE_LIBPNG, test x$have_libpng = xtrue)

if test x$have_libpng = xtrue; then
AC_DEFINE(HAVE_LIBPNG)
fi

])

dnl
dnl FLAC
dnl

AC_DEFUN([GMERLIN_CHECK_FLAC],[

FLAC_REQUIRED="1.2.0"
have_flac="false"

AC_ARG_ENABLE(flac,
[AS_HELP_STRING([--disable-flac],[Disable flac (default: autodetect)])],
[case "${enableval}" in
   yes) test_flac=true ;;
   no)  test_flac=false ;;
esac],[test_flac=true])

if test x$test_flac = xtrue; then

AH_TEMPLATE([HAVE_FLAC], [Enable FLAC])

PKG_CHECK_MODULES(FLAC, flac, have_flac="true", have_flac="false")
fi

AC_SUBST(FLAC_CFLAGS)
AC_SUBST(FLAC_LIBS)
AC_SUBST(FLAC_REQUIRED)

AM_CONDITIONAL(HAVE_FLAC, test x$have_flac = xtrue)

if test x$have_flac = xtrue; then
AC_DEFINE(HAVE_FLAC)
fi

])

dnl
dnl Musepack
dnl

AC_DEFUN([GMERLIN_CHECK_MUSEPACK],[

have_musepack="false"
MUSEPACK_REQUIRED="1.1"

AC_ARG_ENABLE(musepack,
[AS_HELP_STRING([--disable-musepack],[Disable musepack (default: autodetect)])],
[case "${enableval}" in
   yes) test_musepack=true ;;
   no)  test_musepack=false ;;
esac],[test_musepack=true])

if test x$test_musepack = xtrue; then

OLD_CFLAGS=$CFLAGS
OLD_LIBS=$LIBS

LIBS="-lmpcdec"
CFLAGS=""

AH_TEMPLATE([HAVE_MUSEPACK], [Enable Musepack])
AC_MSG_CHECKING(for libmpcdec)

  AC_LINK_IFELSE([AC_LANG_SOURCE([[#include <mpc/mpcdec.h>
    #include <stdio.h>
    int main()
    {
    mpc_reader reader;
    mpc_demux_init(&reader);
    return 0;
    }
]])],[
    # program could be run
    have_musepack="true"
    AC_MSG_RESULT(yes)
    MUSEPACK_CFLAGS=$CFLAGS
    MUSEPACK_LIBS=$LIBS
  ],[
    # program could not be run
    AC_MSG_RESULT(no)
  ])

CFLAGS=$OLD_CFLAGS
LIBS=$OLD_LIBS

fi

AC_SUBST(MUSEPACK_CFLAGS)
AC_SUBST(MUSEPACK_LIBS)
AC_SUBST(MUSEPACK_REQUIRED)

AM_CONDITIONAL(HAVE_MUSEPACK, test x$have_musepack = xtrue)

if test x$have_musepack = xtrue; then
AC_DEFINE(HAVE_MUSEPACK)
fi

])

dnl
dnl MAD
dnl

AC_DEFUN([GMERLIN_CHECK_MAD],[

MAD_REQUIRED="0.15.0"
AH_TEMPLATE([HAVE_MAD], [Enable MAD])
have_mad="false"

AC_ARG_ENABLE(mad,
[AS_HELP_STRING([--disable-mad],[Disable libmad (default: autodetect)])],
[case "${enableval}" in
   yes) test_mad=true ;;
   no)  test_mad=false ;;
esac],[test_mad=true])

if test x$test_mad = xtrue; then

OLD_CFLAGS=$CFLAGS
OLD_LIBS=$LIBS
   
LIBS="-lmad"
CFLAGS=""

PKG_CHECK_MODULES(MAD, mad >= $MAD_REQUIRED, have_mad="true", have_mad="false")

CFLAGS=$OLD_CFLAGS
LIBS=$OLD_LIBS

fi

AC_SUBST(MAD_CFLAGS)
AC_SUBST(MAD_LIBS)
AC_SUBST(MAD_REQUIRED)

AM_CONDITIONAL(HAVE_MAD, test x$have_mad = xtrue)

if test x$have_mad = xtrue; then
AC_DEFINE(HAVE_MAD)
fi

])

dnl
dnl liba52
dnl

AC_DEFUN([GMERLIN_CHECK_LIBA52],[

AH_TEMPLATE([HAVE_LIBA52], [Enable liba52])
have_liba52="false"

AC_ARG_ENABLE(liba52,
[AS_HELP_STRING([--disable-liba52],[Disable liba52 (default: autodetect)])],
[case "${enableval}" in
   yes) test_liba52=true ;;
   no)  test_liba52=false ;;
esac],[test_liba52=true])

if test x$test_liba52 = xtrue; then

OLD_CFLAGS=$CFLAGS
OLD_LIBS=$LIBS
   
LIBS="-la52 -lm"
CFLAGS=""
LIBA52_REQUIRED="0.7.4"
AC_MSG_CHECKING([for liba52])

  AC_LINK_IFELSE([
    AC_LANG_SOURCE([[
    #include <inttypes.h>
    #include <a52dec/a52.h>
    int main() {
      a52_state_t * state = a52_init(0);
      return 0;
      }
    ]])
  ],[
    # program could be run
    have_liba52="true"
    AC_MSG_RESULT(yes)
    LIBA52_CFLAGS=$CFLAGS
    LIBA52_LIBS=$LIBS
  ],[
    # program could not be run
    AC_MSG_RESULT(no)
  ]
)

CFLAGS=$OLD_CFLAGS
LIBS=$OLD_LIBS

fi

AC_SUBST(LIBA52_CFLAGS)
AC_SUBST(LIBA52_LIBS)
AC_SUBST(LIBA52_REQUIRED)

AM_CONDITIONAL(HAVE_LIBA52, test x$have_liba52 = xtrue)

if test x$have_liba52 = xtrue; then
AC_DEFINE(HAVE_LIBA52)
fi

])

dnl
dnl CDrom support
dnl

AC_DEFUN([GMERLIN_CHECK_CDIO],[

AH_TEMPLATE([HAVE_CDIO], [ libcdio found ])

have_cdio="false"
CDIO_REQUIRED="0.79"

AC_ARG_ENABLE(libcdio,
[AS_HELP_STRING([--disable-libcdio],[Disable libcdio (default: autodetect)])],
[case "${enableval}" in
   yes) test_cdio=true ;;
   no)  test_cdio=false ;;
esac],[test_cdio=true])

if test x$test_cdio = xtrue; then
PKG_CHECK_MODULES(CDIO, libcdio >= $CDIO_REQUIRED, have_cdio="true", have_cdio="false")
fi

AM_CONDITIONAL(HAVE_CDIO, test x$have_cdio = xtrue)
AC_SUBST(CDIO_REQUIRED)

if test "x$have_cdio" = "xtrue"; then
AC_DEFINE([HAVE_CDIO])
fi

])

dnl
dnl libudf
dnl

AC_DEFUN([GMERLIN_CHECK_LIBUDF],[

AH_TEMPLATE([HAVE_LIBUDF], [ libudf found ])

AC_MSG_CHECKING([for libudf])
if test "x$have_cdio" = "xtrue"; then

OLD_CFLAGS=$CFLAGS
OLD_LIBS=$LIBS

CFLAGS=$CDIO_CFLAGS
LIBS=$CDIO_LIBS

AC_CHECK_LIB(udf, udf_open, have_libudf=true, have_libudf=false)

if test "x$have_libudf" = "xtrue"; then
AC_DEFINE([HAVE_LIBUDF])
LIBUDF_LIBS="-ludf"
AC_MSG_RESULT([Found])
else
LIBUDF_LIBS=""
AC_MSG_RESULT([Not found])
fi

CFLAGS=$OLD_CFLAGS
LIBS=$OLD_LIBS

else
AC_MSG_RESULT([Not found (libcdio missing)])
fi

AC_SUBST(LIBUDF_LIBS)

])

dnl
dnl libdca
dnl

AC_DEFUN([GMERLIN_CHECK_DCA],[

AH_TEMPLATE([HAVE_DCA], [ libdca found ])

have_dca="false"
have_dts="false"

DCA_REQUIRED="0.0.2"

AC_ARG_ENABLE(libcda,
[AS_HELP_STRING([--disable-libdca],[Disable libdca (default: autodetect)])],
[case "${enableval}" in
   yes) test_libdca=true ;;
   no)  test_libdca=false ;;
esac],[test_libdca=true])

if test x$test_libdca = xtrue; then
PKG_CHECK_MODULES(DCA, libdca >= $DCA_REQUIRED, have_dca="true", have_dca="false")

if test "x$have_dca" != "xtrue"; then
PKG_CHECK_MODULES(DCA, libdts >= $DCA_REQUIRED, have_dts="true", have_dts="false")

dnl
dnl Check for old dts.h header
dnl

OLD_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CFLAGS $DCA_CFLAGS"
AC_CHECK_HEADERS([dts.h])
CPPFLAGS=$OLD_CPPFLAGS


dnl
dnl Some systems need -ldts_pic
dnl

if test x$have_dts = xtrue; then
have_libdts_pic=false
OLD_CFLAGS=$CFLAGS
OLD_LIBS=$LIBS
CFLAGS=$DCA_CFLAGS
LIBS=`pkg-config --libs-only-L libdts`
LIBS="$LIBS -lm"
AC_CHECK_LIB(dts_pic, dts_init, have_libdts_pic=true, have_libdts_pic=false)

if test x$have_libdts_pic = xtrue; then
DCA_LIBS="$LIBS -ldts_pic"
fi

CFLAGS=$OLD_CFLAGS
LIBS=$OLD_LIBS

have_dca="true"

fi
fi
fi

AM_CONDITIONAL(HAVE_DCA, test x$have_dca = xtrue)
AC_SUBST(DCA_REQUIRED)

if test "x$have_dca" = "xtrue"; then
AC_DEFINE([HAVE_DCA])
fi

])

dnl
dnl libjpeg
dnl

AC_DEFUN([GMERLIN_CHECK_LIBJPEG],[

AH_TEMPLATE([HAVE_LIBJPEG],
            [Do we have libjpeg installed?])

have_libjpeg=false
JPEG_REQUIRED="6b"

AC_ARG_ENABLE(libjpeg,
[AS_HELP_STRING([--disable-libjpeg],[Disable libjpeg (default: autodetect)])],
[case "${enableval}" in
   yes) test_libjpeg=true ;;
   no)  test_libjpeg=false ;;
esac],[test_libjpeg=true])

if test x$test_libjpeg = xtrue; then

OLD_LIBS=$LIBS
LIBS="$LIBS -ljpeg"

AC_MSG_CHECKING(for libjpeg)
AC_TRY_LINK([#include <stdio.h>
             #include <jpeglib.h>],[
struct jpeg_decompress_struct cinfo; jpeg_create_decompress(&cinfo);
            ],[have_libjpeg=true])
case $have_libjpeg in
  true) AC_DEFINE(HAVE_LIBJPEG)
        AC_MSG_RESULT(yes)
        JPEG_LIBS=$LIBS;;
  false) AC_MSG_RESULT(no); JPEG_LIBS=""; JPEG_CFLAGS="";;
  * ) AC_MSG_RESULT("Somethings wrong: $have_libjpeg") ;;
esac

LIBS=$OLD_LIBS

fi

AC_SUBST(JPEG_LIBS)
AC_SUBST(JPEG_CFLAGS)
AC_SUBST(JPEG_REQUIRED)
AM_CONDITIONAL(HAVE_LIBJPEG, test x$have_libjpeg = xtrue)

])

dnl
dnl Video4linux2
dnl

AC_DEFUN([GMERLIN_CHECK_V4L2],[

AH_TEMPLATE([HAVE_V4L2], [Enable v4l2])
	     
have_v4l2=false
AC_ARG_ENABLE(v4l2,
              AS_HELP_STRING(--disable-v4l2, [Disable Video4Linux (default: autodetect)]),
              [case "${enableval}" in
                 yes) test_v4l2=true ;;
                 no) test_v4l2=false ;;
               esac],
	       test_v4l2=true)

if test x$test_v4l2 = xtrue; then
AC_CHECK_HEADERS(linux/videodev2.h, have_v4l2=true)
fi

AM_CONDITIONAL(HAVE_V4L2, test x$have_v4l2 = xtrue)

if test x$have_v4l2 = xtrue; then
AC_DEFINE(HAVE_V4L2)
fi


])

dnl
dnl Check for opus codec
dnl

AC_DEFUN([GMERLIN_CHECK_OPUS],[

AH_TEMPLATE([HAVE_OPUS],
            [Do we have libopus installed?])

have_opus="false"

OPUS_REQUIRED="1.0.0"

AC_ARG_ENABLE(opus,
[AS_HELP_STRING([--disable-opus],[Disable opus (default: autodetect)])],
[case "${enableval}" in
   yes) test_opus=true ;;
   no)  test_opus=false ;;
esac],[test_opus=true])

if test x$test_opus = xtrue; then

PKG_CHECK_MODULES(OPUS, opus, have_opus="true", have_opus="false")
fi

AC_SUBST(OPUS_REQUIRED)
AC_SUBST(OPUS_LIBS)
AC_SUBST(OPUS_CFLAGS)

AM_CONDITIONAL(HAVE_OPUS, test x$have_opus = xtrue)

if test "x$have_opus" = "xtrue"; then
AC_DEFINE([HAVE_OPUS])
fi

])

dnl
dnl X11
dnl

AC_DEFUN([GMERLIN_CHECK_X11],[

have_x="false"

X_CFLAGS=""
X_LIBS=""

AH_TEMPLATE([HAVE_XLIB],
            [Do we have xlib installed?])

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
  AC_DEFINE([HAVE_XLIB])
fi


AC_SUBST(X_CFLAGS)
AC_SUBST(X_LIBS)
AM_CONDITIONAL(HAVE_X11, test x$have_x = xtrue)

])


dnl
dnl libva
dnl

AC_DEFUN([GMERLIN_CHECK_LIBVA],[

AH_TEMPLATE([HAVE_LIBVA],
            [Do we have libva installed?])
AH_TEMPLATE([HAVE_LIBVA_X11],
            [Do we have libva (x11) installed?])

have_libva="false"
have_libva_glx="false"
have_libva_x11="false"

LIBVA_CFLAGS=""
LIBVA_LIBS=""

AC_ARG_ENABLE(libva,
[AS_HELP_STRING([--disable-libva],[Disable libva (default: autodetect)])],
[case "${enableval}" in
   yes) test_libva=true ;;
   no)  test_libva=false ;;
esac],[test_libva=true])

if test x$have_x != xtrue; then
test_libva="false"
fi

if test x$test_libva = xtrue; then
PKG_CHECK_MODULES(LIBVA_BASE, libva, have_libva="true", have_libva="false")
fi

if test "x$have_libva" = "xtrue"; then
LIBVA_CFLAGS=$LIBVA_BASE_CFLAGS
LIBVA_LIBS=$LIBVA_BASE_LIBS

if test x$have_x = xtrue; then
PKG_CHECK_MODULES(LIBVA_X11, libva-x11, have_libva_x11="true", have_libva_x11="false")
fi

if test "x$have_libva_x11" = "xtrue"; then
LIBVA_CFLAGS="$LIBVA_CFLAGS $LIBVA_X11_CFLAGS"
LIBVA_LIBS="$LIBVA_LIBS $LIBVA_X11_LIBS"
else
have_libva="false"
fi

fi

AC_SUBST(LIBVA_LIBS)
AC_SUBST(LIBVA_CFLAGS)

AM_CONDITIONAL(HAVE_LIBVA, test x$have_libva = xtrue)
AM_CONDITIONAL(HAVE_LIBVA_X11, test x$have_libva_x11 = xtrue)

if test "x$have_libva" = "xtrue"; then
AC_DEFINE([HAVE_LIBVA])
fi

if test "x$have_libva_x11" = "xtrue"; then
AC_DEFINE([HAVE_LIBVA_X11])
fi

])
