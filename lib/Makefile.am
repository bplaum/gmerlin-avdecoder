
lib_LTLIBRARIES = libgmerlin_avdec.la

AM_CPPFLAGS = -I$(top_srcdir)/include

noinst_HEADERS = pnm.h targa.h

if HAVE_OGG
ogg_cflags = @OGG_CFLAGS@
ogg_libs =   @OGG_LIBS@
ogg_sources = demux_ogg.c
else
ogg_cflags = 
ogg_libs =   
ogg_sources =
endif


if HAVE_V4L2
v4l_sources = video_v4l2_m2m.c
else
v4l_sources = 
endif

if HAVE_VORBIS
vorbis_cflags = @VORBIS_CFLAGS@
vorbis_libs =   @VORBIS_LIBS@ @VORBISENC_LIBS@
vorbis_sources = audio_vorbis.c parse_vorbis.c
else
vorbis_cflags =
vorbis_libs =
vorbis_sources =
endif

if HAVE_OPUS
opus_cflags = @OPUS_CFLAGS@
opus_libs =   @OPUS_LIBS@
opus_sources = parse_opus.c audio_opus.c
else
opus_cflags =
opus_libs =
opus_sources =
endif

if HAVE_LIBA52
a52_cflags = @LIBA52_CFLAGS@
a52_libs =   @LIBA52_LIBS@
a52_sources = audio_a52.c demux_a52.c
else
a52_cflags =
a52_libs =
a52_sources =
endif

if HAVE_LIBPNG
png_cflags =  @PNG_CFLAGS@
png_libs =    @PNG_LIBS@
png_sources = video_png.c pngreader.c
else
png_cflags =
png_libs =
png_sources =
endif

if HAVE_OPENJPEG
openjpeg_cflags =  @OPENJPEG_CFLAGS@
openjpeg_libs =    @OPENJPEG_LIBS@
openjpeg_sources = video_openjpeg.c
else
openjpeg_cflags =
openjpeg_libs =
openjpeg_sources =
endif


if HAVE_LIBTIFF
tiff_cflags =  @TIFF_CFLAGS@
tiff_libs =    @TIFF_LIBS@
tiff_sources = video_tiff.c
else
tiff_cflags =
tiff_libs =
tiff_sources =
endif

if HAVE_FLAC
flac_cflags =  @FLAC_CFLAGS@
flac_libs =    @FLAC_LIBS@
flac_sources = audio_flac.c
else
flac_cflags =
flac_libs =
flac_sources =
endif

if HAVE_MAD
mad_cflags =  @MAD_CFLAGS@
mad_libs =    @MAD_LIBS@
mad_sources = audio_mad.c
else
mad_cflags =
mad_libs =
mad_sources =
endif

if HAVE_DCA
dca_cflags =  @DCA_CFLAGS@
dca_libs =    @DCA_LIBS@
dca_sources = audio_dca.c parse_dca.c
else
dca_cflags =
dca_libs =
dca_sources =
endif


if HAVE_MUSEPACK
musepack_cflags =  @MUSEPACK_CFLAGS@
musepack_libs =    @MUSEPACK_LIBS@
musepack_sources = demux_musepack.c
else
musepack_cflags =
musepack_libs =
musepack_sources =
endif

if HAVE_AVFORMAT
avformat_cflags =  @AVFORMAT_CFLAGS@
avformat_libs =    @AVFORMAT_LIBS@
avformat_sources = demux_ffmpeg.c
else
avformat_cflags =
avformat_libs =
avformat_sources =
endif


if HAVE_AVCODEC
avcodec_cflags =  @AVCODEC_CFLAGS@
avcodec_libs =    @AVCODEC_LIBS@ -lavutil
avcodec_sources = audio_ffmpeg.c video_ffmpeg.c bsf_adts.c
else
avcodec_cflags =
avcodec_libs =
avcodec_sources =
endif

if HAVE_CDIO
cdio_cflags =  @CDIO_CFLAGS@
cdio_libs =    @CDIO_LIBS@ @LIBUDF_LIBS@
else
cdio_cflags =
cdio_libs =
endif

if HAVE_DVDREAD
dvdread_cflags  = @DVDREAD_CFLAGS@
dvdread_libs    = @DVDREAD_LIBS@
else
dvdread_cflags  =
dvdread_libs    =
endif

if HAVE_SAMBA
samba_cflags =  @SAMBA_CFLAGS@
samba_libs =    @SAMBA_LIBS@
samba_sources = in_smb.c
else
samba_cflags =
samba_libs =
samba_sources =
endif

if HAVE_THEORADEC
theora_cflags  = @THEORADEC_CFLAGS@
theora_libs    = @THEORADEC_LIBS@
theora_sources = video_theora.c
else
theora_cflags  =
theora_libs    =
theora_sources =
endif

if HAVE_SCHROEDINGER
schroedinger_cflags  = @SCHROEDINGER_CFLAGS@
schroedinger_libs    = @SCHROEDINGER_LIBS@
schroedinger_sources = video_schroedinger.c
else
schroedinger_cflags  =
schroedinger_libs    =
schroedinger_sources =
endif


if HAVE_SPEEX
speex_cflags  = @SPEEX_CFLAGS@
speex_libs    = @SPEEX_LIBS@
speex_sources = audio_speex.c parse_speex.c
else
speex_cflags  =
speex_libs    =
speex_sources =
endif


if HAVE_LIBVA
vaapi_sources = 
vaapi_libs = @LIBVA_LIBS@ @X_LIBS@
vaapi_cflags = @LIBVA_CFLAGS@ @X_CFLAGS@
else
vaapi_sources =
vaapi_libs = 
vaapi_cflags =
endif


if HAVE_LINUXDVB
dvb_sources = dvb_channels.c
else 	 
dvb_sources = 	 
endif	

if HAVE_LIBGSM
gsm_sources = audio_gsm.c
gsm_subdirs = GSM610
gsm_libs = $(top_builddir)/lib/GSM610/libgsm.la
else
gsm_sources =
gsm_subdirs =
gsm_libs = 
endif  


SUBDIRS = $(win32_subdirs) $(gsm_subdirs)

libgmerlin_avdec_la_LDFLAGS=-export-dynamic -version-info @LTVERSION_CURRENT@:@LTVERSION_REVISION@:@LTVERSION_AGE@ @GMERLIN_LIB_LDFLAGS@

libgmerlin_avdec_la_LIBADD= \
$(vorbis_libs) \
$(opus_libs) \
$(ogg_libs) \
$(win32_libs) \
$(png_libs) \
$(openjpeg_libs) \
$(tiff_libs) \
$(dvdread_libs) \
$(flac_libs) \
$(mad_libs) \
$(dca_libs) \
$(musepack_libs) \
$(avformat_libs) \
$(a52_libs) \
$(avcodec_libs) \
$(cdio_libs) \
$(samba_libs) \
$(theora_libs) \
$(schroedinger_libs) \
$(speex_libs) \
$(libpostproc_libs) \
$(libswscale_libs) \
$(vaapi_libs) \
$(gsm_libs) \
@ICONV_LIBS@ \
@LIBINTL@ \
-lm

AM_CFLAGS = \
-DLOCALE_DIR=\"$(localedir)\" \
"-I/usr/include" \
@LIBBGAV_CFLAGS@ \
@OPT_CFLAGS@ \
$(vorbis_cflags) \
$(opus_cflags) \
$(ogg_cflags) \
$(mad_cflags) \
$(png_cflags) \
$(openjpeg_cflags) \
$(tiff_cflags) \
$(flac_cflags) \
$(a52_cflags) \
$(dca_cflags) \
$(musepack_cflags) \
$(avformat_cflags) \
$(avcodec_cflags) \
$(cdio_cflags) \
$(mjpegtools_cflags) \
$(samba_cflags) \
$(theora_cflags) \
$(schroedinger_cflags) \
$(speex_cflags) \
$(libpostproc_cflags) \
$(libswscale_cflags) \
$(vaapi_cflags)

libgmerlin_avdec_la_SOURCES = \
$(vorbis_sources) \
$(opus_sources) \
$(ogg_sources) \
$(png_sources) \
$(openjpeg_sources) \
$(tiff_sources) \
$(win32_sources) \
$(mad_sources) \
$(musepack_sources) \
$(avformat_sources) \
$(a52_sources) \
$(dca_sources) \
$(flac_sources) \
$(avcodec_sources) \
$(mjpegtools_sources) \
$(samba_sources) \
$(theora_sources) \
$(schroedinger_sources) \
$(speex_sources) \
$(dvb_sources) \
$(vaapi_sources) \
$(v4l_sources) \
$(gsm_sources) \
RTjpeg.c \
a52_header.c \
adts_header.c \
apetag.c \
audio.c \
audio_gavf.c \
audio_gavl.c \
audio_pcm.c \
audioparser.c \
asmrp.c \
base64.c \
bgav.c \
bitstream.c \
bsf.c \
bsf_avcc.c \
bytebuffer.c \
chapterlist.c \
charset.c \
cavs_header.c \
codecs.c \
cue.c \
demuxer.c \
demux_4xm.c \
demux_8svx.c \
demux_adif.c \
demux_adts.c \
demux_aiff.c \
demux_ape.c \
demux_asf.c \
demux_avi.c \
demux_avs.c \
demux_au.c \
demux_cue.c \
demux_daud.c \
demux_dsicin.c \
demux_dv.c \
demux_dxa.c \
demux_flac.c \
demux_fli.c \
demux_flv.c \
demux_gif.c \
demux_gsm.c \
demux_gxf.c \
demux_image.c \
demux_ircam.c \
demux_rawaudio.c \
demux_sphere.c \
demux_matroska.c \
demux_mpegaudio.c \
demux_mpegps.c \
demux_mpegts.c \
demux_mpegvideo.c \
demux_mtv.c \
demux_mxf.c \
demux_nsv.c \
demux_nuv.c \
demux_p2xml.c \
demux_psxstr.c \
demux_quicktime.c \
demux_r3d.c \
demux_ra.c \
demux_rm.c \
demux_roq.c \
demux_shorten.c \
demux_smacker.c \
demux_smaf.c \
demux_smjpeg.c \
demux_sol.c \
demux_tiertex.c \
demux_thp.c \
demux_tta.c \
demux_vivo.c \
demux_voc.c \
demux_vmd.c \
demux_vqa.c \
demux_wav.c \
demux_wavpack.c \
demux_wve.c \
demux_y4m.c \
device.c \
dirac_header.c \
dvframe.c \
fileindex.c \
flac_header.c \
formattracker.c \
frametype.c \
h264_header.c \
hls.c \
http.c \
id3v1.c \
id3v2.c \
in_cb.c \
in_dvb_linux.c \
in_dvd.c \
in_file.c \
in_ftp.c \
in_http.c \
in_memory.c \
in_mms.c \
in_pnm.c \
in_vcd.c \
input.c \
keyframetable.c \
languages.c \
matroska.c \
md5.c \
metadata.c \
mpa_header.c \
mpeg4_header.c \
mpegts_common.c \
mpv_header.c \
mms.c \
mxf.c \
nanosoft.c \
options.c \
opus_header.c \
os.c \
os_inet_aton.c \
packetbuffer.c \
packetpool.c \
packettimer.c \
packet.c \
parse_a52.c \
parse_adts.c \
parse_cavs.c \
parse_dirac.c \
parse_dv.c \
parse_dvdsub.c \
parse_flac.c \
parse_h264.c \
parse_jpeg.c \
parse_mjpa.c \
parse_mpeg4.c \
parse_mpv.c \
parse_mpegaudio.c \
parse_png.c \
parse_vc1.c \
parse_vp8.c \
parse_vp9.c \
pes_header.c \
pnm.c \
ptscache.c \
qt_atom.c \
qt_chan.c \
qt_cmov.c \
qt_dinf.c \
qt_dref.c \
qt_edts.c \
qt_elst.c \
qt_enda.c \
qt_esds.c \
qt_fiel.c \
qt_frma.c \
qt_ftab.c \
qt_glbl.c \
qt_gmhd.c \
qt_gmin.c \
qt_hdlr.c \
qt_language.c \
qt_mdhd.c \
qt_mdia.c \
qt_mfhd.c \
qt_minf.c \
qt_moof.c \
qt_moov.c \
qt_mvhd.c \
qt_nmhd.c \
qt_palette.c \
qt_pasp.c \
qt_rdrf.c \
qt_rmda.c \
qt_rmra.c \
qt_stbl.c \
qt_stco.c \
qt_stsc.c \
qt_stsd.c \
qt_stss.c \
qt_stsz.c \
qt_stts.c \
qt_tcmi.c \
qt_tfdt.c \
qt_tfhd.c \
qt_timecode.c \
qt_tkhd.c \
qt_tmcd.c \
qt_traf.c \
qt_trak.c \
qt_tref.c \
qt_trun.c \
qt_udta.c \
qt_useratoms.c \
qt_utils.c \
qt_wave.c \
redirect.c \
rmff.c \
r_asx.c \
r_m3u.c \
r_pls.c \
r_qtl.c \
r_rtsptext.c \
r_ref.c \
r_smil.c \
rtcp.c \
rtp.c \
rtpbuffer.c \
sampleseek.c \
seek.c \
sdp.c \
stream.c \
streamdecoder.c \
subovl_dvd.c \
subread.c \
subtitle.c \
subtitleconverter.c \
superindex.c \
targa.c \
tcp.c \
timecode.c \
track.c \
tracktable.c \
translation.c \
udp.c \
utils.c \
vc1_header.c \
video.c \
videoparser.c \
video_gavf.c \
video_gavl.c \
video_qtraw.c \
video_aviraw.c \
video_tga.c \
video_y4m.c \
video_yuv.c \
video_rtjpeg.c \
vorbis_comment.c \
xing.c \
yml.c

