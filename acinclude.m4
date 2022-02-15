dnl AC_TRY_CFLAGS (CFLAGS, [ACTION-IF-WORKS], [ACTION-IF-FAILS])
dnl check if $CC supports a given set of cflags
AC_DEFUN([AC_TRY_CFLAGS],
    [AC_MSG_CHECKING([if $CC supports $1 flags])
    SAVE_CFLAGS="$CFLAGS"
    CFLAGS="$1"
    AC_TRY_COMPILE([],[],[ac_cv_try_cflags_ok=yes],[ac_cv_try_cflags_ok=no])
    CFLAGS="$SAVE_CFLAGS"
    AC_MSG_RESULT([$ac_cv_try_cflags_ok])
    if test x"$ac_cv_try_cflags_ok" = x"yes"; then
        ifelse([$2],[],[:],[$2])
    else
        ifelse([$3],[],[:],[$3])
    fi])

dnl Extracted from automake-1.5 and sligtly modified for Xine usage.
dnl Daniel Caujolle-Bert <segfault@club-internet.fr>
 
# Figure out how to run the assembler.
 
# AM_PROG_AS_MOD
AC_DEFUN([AM_PROG_AS_MOD],
[# By default we simply use the C compiler to build assembly code.
AC_REQUIRE([AC_PROG_CC])
: ${CCAS='$(CC)'}
# Set CCASFLAGS if not already set.
: ${CCASFLAGS='$(CFLAGS)'}
# Set ASCOMPILE if not already set.
if test $CCAS = '$'CC; then
: ${CCASCOMPILE='$(LIBTOOL) --mode=compile $(CCAS) $(AM_ASFLAGS) $(CCASFLAGS) -c'}
else
: ${CCASCOMPILE='$(LIBTOOL) --mode=compile $(CCAS) $(AM_ASFLAGS) $(CCASFLAGS)'}
fi
AC_SUBST(CCAS)
AC_SUBST(CCASFLAGS)
AC_SUBST(CCASCOMPILE)])
