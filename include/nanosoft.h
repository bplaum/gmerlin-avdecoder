/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
 *
 * Copyright (c) 2001 - 2012 Members of the Gmerlin project
 * gmerlin-general@lists.sourceforge.net
 * http://gmerlin.sourceforge.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * *****************************************************************/

/* Handling of common redmond stuff */

/* GUID: For codec identification and asf files */

typedef struct
  {
  uint32_t v1;
  uint16_t v2;
  uint16_t v3;
  uint8_t  v4[8];
  } bgav_GUID_t;

void bgav_GUID_dump(bgav_GUID_t * g);
int bgav_GUID_equal(const bgav_GUID_t * g1, const bgav_GUID_t * g2);
int bgav_GUID_read(bgav_GUID_t * ret, bgav_input_context_t * input);
int bgav_GUID_get(bgav_GUID_t * ret, bgav_input_context_t * input);
void bgav_GUID_from_ptr(bgav_GUID_t * ret, uint8_t * ptr);

/* Stuff from redmont */

typedef struct
  {
  uint32_t  biSize;  /* sizeof(BITMAPINFOHEADER) */
  uint32_t  biWidth;
  uint32_t  biHeight;
  uint16_t  biPlanes; /* unused */
  uint16_t  biBitCount;
  uint32_t  biCompression; /* fourcc of image */
  uint32_t  biSizeImage;   /* size of image. For uncompressed images */
                           /* ( biCompression 0 or 3 ) can be zero.  */
  
  uint32_t  biXPelsPerMeter; /* unused */
  uint32_t  biYPelsPerMeter; /* unused */
  uint32_t  biClrUsed;       /* valid only for palettized images. */
  /* Number of colors in palette. */
  uint32_t  biClrImportant;
  } bgav_BITMAPINFOHEADER_t;

void bgav_BITMAPINFOHEADER_read(bgav_BITMAPINFOHEADER_t * ret, uint8_t ** data);
void bgav_BITMAPINFOHEADER_dump(bgav_BITMAPINFOHEADER_t * ret);
void bgav_BITMAPINFOHEADER_get_format(bgav_BITMAPINFOHEADER_t * bh,
                                      bgav_stream_t * f);
void bgav_BITMAPINFOHEADER_set_format(bgav_BITMAPINFOHEADER_t * bh,
                                      bgav_stream_t * f);
uint32_t bgav_BITMAPINFOHEADER_get_fourcc(bgav_BITMAPINFOHEADER_t * bh);


/* Waveformat (resembles the evolution of nanosoft speficifications) */

typedef enum
  {
    BGAV_WAVEFORMAT_WAVEFORMAT,
    BGAV_WAVEFORMAT_PCMWAVEFORMAT,
    BGAV_WAVEFORMAT_WAVEFORMATEX,
    BGAV_WAVEFORMAT_WAVEFORMATEXTENSIBLE,
  } bgav_WAVEFORMAT_type_t;

typedef struct
  {
  bgav_WAVEFORMAT_type_t type;

  struct
    {
    struct
      {
      uint16_t  wFormatTag;     /* value that identifies compression format */
      uint16_t  nChannels;
      uint32_t  nSamplesPerSec;
      uint32_t  nAvgBytesPerSec;
      uint16_t  nBlockAlign;    /* size of a data sample */
      } WAVEFORMAT;
    struct
      {
      uint16_t  wBitsPerSample;
      } PCMWAVEFORMAT;
    struct
      {
      uint16_t  cbSize;         /* size of format-specific data */
      uint8_t * ext_data;
      int       ext_size; /* Can be different from cbSize for WAVEFORMATEXTENSIBLE */
      } WAVEFORMATEX;
    struct
      {
      union
        {
        uint16_t wValidBitsPerSample;
        uint16_t wSamplesPerBlock;
        uint16_t wReserved;
        } Samples;
      uint32_t    dwChannelMask; 
      bgav_GUID_t SubFormat;
      } WAVEFORMATEXTENSIBLE;
    } f;
  } bgav_WAVEFORMAT_t;

void bgav_WAVEFORMAT_read(bgav_WAVEFORMAT_t * ret, uint8_t * data, int size);
void bgav_WAVEFORMAT_dump(bgav_WAVEFORMAT_t * ret);
void bgav_WAVEFORMAT_get_format(bgav_WAVEFORMAT_t * wf, bgav_stream_t * f);
void bgav_WAVEFORMAT_set_format(bgav_WAVEFORMAT_t * wf, bgav_stream_t * f);
void bgav_WAVEFORMAT_free(bgav_WAVEFORMAT_t * wf);


/* RIFF Info (used in AVI and WAV) */

typedef struct
  {
  char * IARL; // Archival Location. Indicates where the subject of the file is archived.
  char * IART; // Artist. Lists the artist of the original subject of the file. For example, "Michaelangelo."
  char * ICMS; // Commissioned. Lists the name of the person or organization that commissioned the subject of
               // the file. For example, "Pope Julian II."
  char * ICMT; // Comments. Provides general comments about the file or the subject of the file. If the
               // comment is several sentences long, end each sentence with a period. Do not include newline
               // characters.
  char * ICOP; // Copyright. Records the copyright information for the file. For example,
               // "Copyright Encyclopedia International 1991." If there are multiple copyrights, separate them
               // by a semicolon followed by a space.
  char * ICRD; // Creation date. Specifies the date the subject of the file was created. List dates in
               // year-month-day format, padding one-digit months and days with a zero on the left. For example,
               // "1553-05-03" for May 3, 1553.
  char * ICRP; // Cropped. Describes whether an image has been cropped and, if so, how it was cropped. For example,
               // "lower right corner."
  char * IDIM; // Dimensions. Specifies the size of the original subject of the file. For example,
               // "8.5 in h, 11 in w."
  char * IDPI; // Dots Per Inch. Stores dots per inch setting of the digitizer used to produce the file, such as
               // "300."
  char * IENG; // Engineer. Stores the name of the engineer who worked on the file. If there are multiple engineers,
               // separate the names by a semicolon and a blank. For example, "Smith, John; Adams, Joe."
  char * IGNR; // Genre. Describes the original work, such as, "landscape," "portrait," "still life," etc.
  char * IKEY; // Keywords. Provides a list of keywords that refer to the file or subject of the file. Separate
               // multiple keywords with a semicolon and a blank. For example, "Seattle; aerial view; scenery."
  char * ILGT; // Lightness. Describes the changes in lightness settings on the digitizer required to produce the
               // file. Note that the format of this information depends on hardware used.
  char * IMED; // Medium. Describes the original subject of the file, such as, "computer image," "drawing,"
               // "lithograph," and so forth.
  char * INAM; // Name. Stores the title of the subject of the file, such as, "Seattle From Above."
  char * IPLT; // Palette Setting. Specifies the number of colors requested when digitizing an image, such as "256."
  char * IPRD; // Product. Specifies the name of the title the file was originally intended for, such as
               // "Encyclopedia of Pacific Northwest Geography."
  char * ISBJ; // Subject. Describes the conbittents of the file, such as "Aerial view of Seattle."
  char * ISFT; // Software. Identifies the name of the software package used to create the file, such as
               // "Microsoft WaveEdit."
  char * ISHP; // Sharpness. Identifies the changes in sharpness for the digitizer required to produce the file
               // (the format depends on the hardware used).
  char * ISRC; // Source. Identifies the name of the person or organization who supplied the original subject of the
               // file. For example, "Trey Research."
  char * ISRF; // Source Form. Identifies the original form of the material that was digitized, such as "slide,"
               // "paper," "map," and so forth. This is not necessarily the same as IMED.
  char * ITCH; // Technician. Identifies the technician who digitized the subject file. For example, "Smith, John."
  } bgav_RIFFINFO_t;

int bgav_RIFFINFO_probe(bgav_input_context_t * input);

bgav_RIFFINFO_t * bgav_RIFFINFO_read(bgav_input_context_t * input);
bgav_RIFFINFO_t * bgav_RIFFINFO_read_without_header(bgav_input_context_t * input, int size);
 
void bgav_RIFFINFO_dump(bgav_RIFFINFO_t * info);
void bgav_RIFFINFO_destroy(bgav_RIFFINFO_t * info);

void bgav_RIFFINFO_get_metadata(bgav_RIFFINFO_t * info, bgav_metadata_t * m);

