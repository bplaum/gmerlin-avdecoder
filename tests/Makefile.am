AM_CPPFLAGS = -I$(top_srcdir)/include

AM_LDFLAGS = @GMERLIN_EXE_LDFLAGS@

bin_PROGRAMS = bgavdump \
bgavdemux

noinst_PROGRAMS = \
bgavsave \
frametable \
indexdump \
indextest \
mmstest \
vcdtest \
ymltest \
count_frames \
count_samples \
seektest

bgavdump_SOURCES = bgavdump.c
bgavdump_LDADD = $(top_builddir)/lib/libgmerlin_avdec.la

bgavdemux_SOURCES = bgavdemux.c
bgavdemux_LDADD = $(top_builddir)/lib/libgmerlin_avdec.la

frametable_SOURCES = frametable.c
frametable_LDADD = $(top_builddir)/lib/libgmerlin_avdec.la

seektest_SOURCES = seektest.c
seektest_LDADD = $(top_builddir)/lib/libgmerlin_avdec.la


indextest_SOURCES = indextest.c
indextest_LDADD = $(top_builddir)/lib/libgmerlin_avdec.la

indexdump_SOURCES = indexdump.c
indexdump_LDADD = $(top_builddir)/lib/libgmerlin_avdec.la

count_samples_SOURCES = count_samples.c
count_samples_LDADD = $(top_builddir)/lib/libgmerlin_avdec.la

count_frames_SOURCES = count_frames.c
count_frames_LDADD = $(top_builddir)/lib/libgmerlin_avdec.la

vcdtest_SOURCES = vcdtest.c
vcdtest_LDADD = $(top_builddir)/lib/libgmerlin_avdec.la

ymltest_SOURCES = ymltest.c
ymltest_LDADD = $(top_builddir)/lib/libgmerlin_avdec.la

bgavsave_SOURCES = bgavsave.c
bgavsave_LDADD = $(top_builddir)/lib/libgmerlin_avdec.la


mmstest_SOURCES = mmstest.c
mmstest_LDADD = $(top_builddir)/lib/libgmerlin_avdec.la
