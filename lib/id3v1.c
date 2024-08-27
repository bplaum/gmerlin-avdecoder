/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
 *
 * Copyright (c) 2001 - 2024 Members of the Gmerlin project
 * http://github.com/bplaum
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




#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <avdec_private.h>

struct bgav_id3v1_tag_s
  {
  char * title;   /* Song title	 30 characters */
  char * artist;  /* Artist	 30 characters */
  char * album;   /* Album Album 30 characters */
  char * year;    /* Year         4 characters  */
  char * comment; /* Comment     30/28 characters */

  uint8_t genre;
  uint8_t track;
  
  };

static char * get_string(bgav_charset_converter_t * cnv, char * ptr, int max_size)
  {
  char * end;
  
  end = ptr + max_size - 1;

  while(end > ptr)
    {
    if(!isspace(*end) && (*end != '\0'))
      break;
    end--;
    }
  if(end == ptr)
    return NULL;
  end++;

  return bgav_convert_string(cnv, ptr, end - ptr, NULL);
  //  return gavl_strndup(ptr, end);
  }

bgav_id3v1_tag_t * bgav_id3v1_read(bgav_input_context_t * input)
  {
  
  char buffer[128];
  char * pos;
  
  bgav_id3v1_tag_t * ret;
  bgav_charset_converter_t * cnv;
  
  if(bgav_input_read_data(input, (uint8_t*)buffer, 128) < 128)
    return NULL;

  cnv = bgav_charset_converter_create("ISO-8859-1", BGAV_UTF8);
  
  ret = calloc(1, sizeof(*ret));
  
  pos = buffer + 3;
  ret->title = get_string(cnv, pos, 30);

  pos = buffer + 33;
  ret->artist = get_string(cnv, pos, 30);

  pos = buffer + 63;
  ret->album = get_string(cnv, pos, 30);

  pos = buffer + 93;
  ret->year = get_string(cnv, pos, 4);

  pos = buffer + 97;
  ret->comment = get_string(cnv, pos, 30);
  if(ret->comment && strlen(ret->comment) <= 28)
    ret->track = buffer[126];
  ret->genre = buffer[127];

  bgav_charset_converter_destroy(cnv);

  return ret;
  }

int bgav_id3v1_probe(bgav_input_context_t * input)
  {
  uint8_t probe_data[3];
  if(bgav_input_get_data(input, probe_data, 3) < 3)
    return 0;
  if((probe_data[0] == 'T') &&
     (probe_data[1] == 'A') &&
     (probe_data[2] == 'G'))
    return 1;
  return 0;
  }

#define FREE(s) if(s)free(s);

void bgav_id3v1_destroy(bgav_id3v1_tag_t * t)
  {
  FREE(t->title);
  FREE(t->artist);  /* Artist	 30 characters */
  FREE(t->album);   /* Album Album 30 characters */
  FREE(t->year);    /* Year         4 characters  */
  FREE(t->comment); /* Comment     30/28 characters */
  free(t);
  }

#define GENRE_MAX 0x94

static char const * const id3_genres[GENRE_MAX] =
  {
    "Blues", "Classic Rock", "Country", "Dance",
    "Disco", "Funk", "Grunge", "Hip-Hop",
    "Jazz", "Metal", "New Age", "Oldies",
    "Other", "Pop", "R&B", "Rap", "Reggae",
    "Rock", "Techno", "Industrial", "Alternative",
    "Ska", "Death Metal", "Pranks", "Soundtrack",
    "Euro-Techno", "Ambient", "Trip-Hop", "Vocal",
    "Jazz+Funk", "Fusion", "Trance", "Classical",
    "Instrumental", "Acid", "House", "Game",
    "Sound Clip", "Gospel", "Noise", "Alt",
    "Bass", "Soul", "Punk", "Space",
    "Meditative", "Instrumental Pop",
    "Instrumental Rock", "Ethnic", "Gothic",
    "Darkwave", "Techno-Industrial", "Electronic",
    "Pop-Folk", "Eurodance", "Dream",
    "Southern Rock", "Comedy", "Cult",
    "Gangsta Rap", "Top 40", "Christian Rap",
    "Pop/Funk", "Jungle", "Native American",
    "Cabaret", "New Wave", "Psychedelic", "Rave",
    "Showtunes", "Trailer", "Lo-Fi", "Tribal",
    "Acid Punk", "Acid Jazz", "Polka", "Retro",
    "Musical", "Rock & Roll", "Hard Rock", "Folk",
    "Folk/Rock", "National Folk", "Swing",
    "Fast-Fusion", "Bebob", "Latin", "Revival",
    "Celtic", "Bluegrass", "Avantgarde",
    "Gothic Rock", "Progressive Rock",
    "Psychedelic Rock", "Symphonic Rock", "Slow Rock",
    "Big Band", "Chorus", "Easy Listening",
    "Acoustic", "Humour", "Speech", "Chanson",
    "Opera", "Chamber Music", "Sonata", "Symphony",
    "Booty Bass", "Primus", "Porn Groove",
    "Satire", "Slow Jam", "Club", "Tango",
    "Samba", "Folklore", "Ballad", "Power Ballad",
    "Rhythmic Soul", "Freestyle", "Duet",
    "Punk Rock", "Drum Solo", "A Cappella",
    "Euro-House", "Dance Hall", "Goa",
    "Drum & Bass", "Club-House", "Hardcore",
    "Terror", "Indie", "BritPop", "Negerpunk",
    "Polsk Punk", "Beat", "Christian Gangsta Rap",
    "Heavy Metal", "Black Metal", "Crossover",
    "Contemporary Christian", "Christian Rock",
    "Merengue", "Salsa", "Thrash Metal",
    "Anime", "JPop", "Synthpop"
  };

#define CS(src, gavl_name) \
  if(t->src) gavl_dictionary_set_string(m, gavl_name, t->src);


const char * bgav_id3v1_get_genre(int id)
  {
  if(id < GENRE_MAX)
    return id3_genres[id];
  return NULL;
  }

void bgav_id3v1_2_metadata(bgav_id3v1_tag_t * t, gavl_dictionary_t * m)
  {
  CS(title, GAVL_META_TITLE);
  CS(artist, GAVL_META_ARTIST);
  CS(album, GAVL_META_ALBUM);
  CS(year, GAVL_META_YEAR);
  CS(comment, GAVL_META_COMMENT);
  
  if(t->genre < GENRE_MAX)
    gavl_dictionary_set_string(m, GAVL_META_GENRE, bgav_id3v1_get_genre(t->genre));
  if(t->track)
    gavl_dictionary_set_int(m, GAVL_META_GENRE, t->track);
  }
