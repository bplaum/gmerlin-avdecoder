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

#ifndef __BGAV_SDP_H_
#define __BGAV_SDP_H_

typedef struct bgav_sdp_s bgav_sdp_t;

 /* Origin (o=) */

typedef struct
  {
  char * username;        /* NULL if "-" */
  uint64_t   session_id;
  uint64_t   session_version;
  char * network_type;
  char * addr_type;
  char * addr;
  } bgav_sdp_origin_t;

/* Connection description (c=) */

typedef struct
  {
  char *type;
  char *addr;
  uint32_t ttl;
  uint32_t num_addr;
  } bgav_sdp_connection_desc_t;

/* Bandwidth description (b=) */

typedef enum
  {
    BGAV_SDP_BANDWIDTH_MODIFIER_NONE = 0,
    BGAV_SDP_BANDWIDTH_MODIFIER_CT,
    BGAV_SDP_BANDWIDTH_MODIFIER_AS,
    BGAV_SDP_BANDWIDTH_MODIFIER_USER,
  } bgav_sdp_bandwidth_modifier_t;

typedef struct
  {
  bgav_sdp_bandwidth_modifier_t modifier;
  char * user_bandwidth;
  unsigned long bandwidth;
  } bgav_sdp_bandwidth_desc_t;

/* Attribute (a=) */

typedef enum
  {
    BGAV_SDP_TYPE_NONE    = 0,
    BGAV_SDP_TYPE_BOOLEAN = 1,
    BGAV_SDP_TYPE_INT     = 2,
    BGAV_SDP_TYPE_STRING  = 3,
    BGAV_SDP_TYPE_DATA    = 4,
    BGAV_SDP_TYPE_GENERIC = 5, /* Everything else */
  } bgav_sdp_attr_type_t;

typedef struct
  {
  char * name;
  bgav_sdp_attr_type_t type;
  
  union
    {
    char * str;
    void * data;
    int i;
    } val;
  
  int data_len;
  } bgav_sdp_attr_t;

/* Key (k=) */

typedef enum
  {
  BGAV_SDP_KEY_TYPE_NONE = 0,
  BGAV_SDP_KEY_TYPE_CLEAR,
  BGAV_SDP_KEY_TYPE_BASE64,
  BGAV_SDP_KEY_TYPE_URI,
  BGAV_SDP_KEY_TYPE_PROMPT,
  } bgav_sdp_key_type_t;

typedef struct key_desc_t
  {
  bgav_sdp_key_type_t type;
  char *key;
  } bgav_sdp_key_desc_t;

/* Media Description (m=)      */

typedef struct
  {
  /* From the m= line */

  char * media; /* audio, video etc */

  int port;
  int num_ports;

  char * protocol;

  int num_formats;
  char ** formats;
      
  /* Additional stuff */

  char * media_title;                    /* i=* (media title) */
  bgav_sdp_connection_desc_t connection; /* c= */
  bgav_sdp_bandwidth_desc_t  bandwidth;  /* b= */
  bgav_sdp_key_desc_t key;                        /* k= */
  
  /* a= */
  
  int num_attributes;
  bgav_sdp_attr_t * attributes;
  } bgav_sdp_media_desc_t;

/* Whole sdp structure */

struct bgav_sdp_s
  {
  int protocol_version;                  /* v= */

  bgav_sdp_origin_t origin;              /* o= */

  char * session_name;                   /* s= */
  char * session_description;            /* i= */
  char * uri;                            /* u= */
  char * email;                          /* e= */
  char * phone;                          /* p= */
  bgav_sdp_connection_desc_t connection; /* c= */
  bgav_sdp_bandwidth_desc_t  bandwidth;  /* b= */
  /* t=  One or more time descriptions (see below) */
  // z=* (time zone adjustments)
  
  bgav_sdp_key_desc_t key;                /* k=* (encryption key) */
    
  // a=* (zero or more session attribute lines)
  // Zero or more media descriptions (see below)
  
  //  int num_time_descriptions;
  //  time_desc_t * time_descriptions;

  int num_attributes;
  bgav_sdp_attr_t * attributes;

  int num_media;
  bgav_sdp_media_desc_t * media;
  };

/* Obtain attributes */

/*
 *  These functions return FALSE if
 *  there is a type mismatch, or no attribute with the
 *  specified name is found
 */

int bgav_sdp_get_attr_string(bgav_sdp_attr_t * attrs, int num_attrs,
                             const char * name, char ** ret);

int bgav_sdp_get_attr_data(bgav_sdp_attr_t * attrs, int num_attrs,
                           const char * name, uint8_t ** ret, int * size);

int bgav_sdp_get_attr_int(bgav_sdp_attr_t * attrs, int num_attrs,
                          const char * name, int*);

int bgav_sdp_get_attr_rtpmap(bgav_sdp_attr_t * attrs, int num_attrs, int format, char ** ret);
int bgav_sdp_get_attr_fmtp(bgav_sdp_attr_t * attrs, int num_attrs, int format, char ** ret);


/* Init, free */

int bgav_sdp_parse(const bgav_options_t * opt,
                   const char * data, bgav_sdp_t * ret);
void bgav_sdp_free(bgav_sdp_t * s);

/* Dump the entire struct to stderr */

void bgav_sdp_dump(bgav_sdp_t * s);

#endif // __BGAV_SDP_H_
