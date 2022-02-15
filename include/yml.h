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

/* VERY simple and nonstandard xml-like parser */

/*
 *  We need this, because asx files are, according to
 *  Microsoft Documentation, xml-based, but the same doc says,
 *  that tag- and attribute names are case insensitive,
 *  which is explicitely forbidden by the xml-spec.
 *  Since we can't tell libxml to be case insensitive,
 *  we need our own parser.
 *
 *  We prefix everything with yml, so we never forget,
 *  that we don't deal with standard xml here.
 */

typedef struct bgav_yml_attr_s
  {
  struct bgav_yml_attr_s * next;
  char * name;
  char * value;
  } bgav_yml_attr_t;

typedef struct bgav_yml_node_s
  {
  char * name;   /* NULL if we have a pure text node */
  char * pi;     /* Processing info */
  char * str;    /* Text for text nodes              */
  
  bgav_yml_attr_t * attributes; /* Attributes of the node */
  // bgav_yml_attr_t * xml_attributes;  /* Attributes of the <?xml>-node */
  struct bgav_yml_node_s * next;
  struct bgav_yml_node_s * children;
  } bgav_yml_node_t;

bgav_yml_node_t * bgav_yml_parse(bgav_input_context_t * input)
  __attribute__ ((visibility("default")));

void bgav_yml_dump(bgav_yml_node_t *)
  __attribute__ ((visibility("default")));
void bgav_yml_free(bgav_yml_node_t *)
  __attribute__ ((visibility("default")));

const char * bgav_yml_get_attribute(bgav_yml_node_t *, const char * name);
const char * bgav_yml_get_attribute_i(bgav_yml_node_t *, const char * name);

int bgav_yml_probe(bgav_input_context_t * input);

bgav_yml_node_t * bgav_yml_find_by_name(bgav_yml_node_t * yml, const char * name);
bgav_yml_node_t * bgav_yml_find_by_pi(bgav_yml_node_t * yml, const char * pi);

