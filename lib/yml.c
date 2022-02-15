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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <avdec_private.h>


#define PROBE_LEN 512

#define YML_REGEXP "<\\?xml( *[^=]+ *= *[\"'][^\"']*[\"'])* *\\?>|<[:alnum:]+[^ ]*( *[^=]+ *= *[\"'][^\"']*[\"'])* */? *>"

int bgav_yml_probe(bgav_input_context_t * input)
  {
  char * ptr, *end_ptr;
  char probe_data[PROBE_LEN+1];
  int len;
  len = bgav_input_get_data(input, (uint8_t*)probe_data, PROBE_LEN);

  probe_data[len] = '\0';

  ptr = probe_data;

  while(isspace(*ptr) && (*ptr != '\0'))
    ptr++;


  if(*ptr == '\0')
    return 0;
  
  end_ptr = strchr(ptr, '\n');
  if(!end_ptr)
    return 0;
  *end_ptr = '\0';

  if(bgav_match_regexp(ptr, YML_REGEXP))
    return 1;
  return 0;
  }

typedef struct
  {
  bgav_input_context_t * input;
  char * buffer;
  char * buffer_ptr;
  int buffer_size;
  uint32_t buffer_alloc;
  //  int eof;
  } parser_t;

static bgav_yml_node_t * parse_node(parser_t * p, int * is_comment);

static int more_data(parser_t * p)
  {
  int bytes_read = 0;
  while(1)
    {
    if(!bgav_input_read_line(p->input, &p->buffer, &p->buffer_alloc,
                             p->buffer_size, NULL))
      {
      return bytes_read;
      }
    bytes_read = strlen(p->buffer + p->buffer_size);
    p->buffer_size += bytes_read;
    if(bytes_read)
      {
      return bytes_read;
      }
    }
  return 0; /* Never get here */
  }

static void advance(parser_t * p, int bytes)
  {
  if(bytes > p->buffer_size)
    {
    return;
    }
  if(bytes < p->buffer_size)
    memmove(p->buffer, p->buffer + bytes, p->buffer_size - bytes);
  p->buffer_size -= bytes;
  p->buffer[p->buffer_size] = '\0';
  }

static char * find_chars(parser_t * p, const char * chars)
  {
  char * pos = NULL;
  while(1)
    {
    pos = strpbrk(p->buffer, chars);
    if(pos)
      return pos;
    if(!more_data(p))
      break;
    }
  return NULL;
  }

static int skip_space(parser_t * p)
  {
  if(!p->buffer_size)
    {
    if(!more_data(p))
      return 0;
    }
  while(isspace(*(p->buffer)))
    {
    advance(p, 1);
    if(!p->buffer_size)
      {
      if(!more_data(p))
        return 0;
      }
    }
  return 1;
  }

static char * parse_tag_name(parser_t * p)
  {
  char * pos;
  char  * ret;
  pos = find_chars(p, "> /?");
  if(!pos)
    return NULL;
  ret = gavl_strndup(p->buffer, pos);
  advance(p, pos - p->buffer);
  return ret;
  }

static char * parse_attribute_name(parser_t * p)
  {
  char * pos;
  char * ret;
  pos = find_chars(p, "= ");
  if(!pos)
    return NULL;
  ret = gavl_strndup(p->buffer, pos);
  advance(p, pos - p->buffer);
  return ret;
  }

static char * parse_attribute_value(parser_t * p)
  {
  char * ret;
  char * end = NULL;
  char close_seq[3];
  
  if(!p->buffer_size)
    {
    if(!more_data(p))
      return NULL;
    }
  if((p->buffer[0] == '\\') && (p->buffer[1] == '"'))
    {
    close_seq[0] = p->buffer[0];
    close_seq[1] = p->buffer[1];
    close_seq[2] = '\0';
    advance(p, 2);
    }
  else
    {
    close_seq[0] = *(p->buffer);
    close_seq[1] = '\0';
    if((close_seq[0] != '\'') && (close_seq[0] != '"'))
      return NULL;
    advance(p, 1);
    }

  while(1)
    {
    end = strstr(p->buffer, close_seq);

    if(end)
      break;
    if(!more_data(p))
      break;
    }
  

  if(!end)
    return NULL;
  ret = gavl_strndup(p->buffer, end);
  
  advance(p, (int)(end - p->buffer) + strlen(close_seq));
  return ret;
  }

static int parse_text_node(parser_t * p, bgav_yml_node_t * ret)
  {
  char * pos;
  pos = find_chars(p, "<");
  if(!pos)
    return 0;
  ret->str = gavl_strndup(p->buffer, pos);
  advance(p, (int)(pos - p->buffer));
  return 1;
  }

static int parse_attributes(parser_t * p, bgav_yml_attr_t ** ret)
  {
  bgav_yml_attr_t * attr_end;

  /* We have at least one attribute here */

  *ret = calloc(1, sizeof(**ret));
  attr_end = *ret;
  
  attr_end->name = parse_attribute_name(p);
  if(!skip_space(p))
    return 0;
  if(p->buffer[0] != '=')
    return 0;
  advance(p, 1); /* '=' */
  if(!skip_space(p))
    return 0;
  attr_end->value = parse_attribute_value(p);

  while(1)
    {
    skip_space(p);
    if((*(p->buffer) == '>') || (*(p->buffer) == '/') || (*(p->buffer) == '?'))
      return 1;
    else
      {
      attr_end->next = calloc(1, sizeof(*(attr_end->next)));
      attr_end = attr_end->next;
      
      attr_end->name = parse_attribute_name(p);

      if(!skip_space(p))
        return 0;
      if(p->buffer[0] != '=')
        return 0;
      advance(p, 1); /* '=' */
      if(!skip_space(p))
        return 0;
      attr_end->value = parse_attribute_value(p);
      }
    }
  return 1;
  }

static int parse_children(parser_t * p, bgav_yml_node_t * ret)
  {
  int is_comment;
  bgav_yml_node_t * child_end = NULL;
  bgav_yml_node_t * new_node;
  while(1)
    {
    if(p->buffer_size < 2)
      {
      if(!more_data(p))
        return 0;
      }
    if((p->buffer[0] == '<') && (p->buffer[1] == '/'))
      break;
    
    new_node = parse_node(p, &is_comment);

    if(!new_node)
      {
      if(!is_comment)
        return 0;
      }
    else
      {
      if(!ret->children)
        {
        ret->children = new_node;
        child_end = new_node;
        }
      else
        {
        child_end->next = new_node;
        child_end = child_end->next;
        }
      }
    }
  return 1;
  }

static int parse_tag_node(parser_t * p, bgav_yml_node_t * ret)
  {
  char * pos;

  if(!skip_space(p))
    return 0;
  if(*(p->buffer) != '<')
    return 0;

  advance(p, 1); /* < */

  if(p->buffer_size < 4)
    {
    if(!more_data(p))
      return 0;
    }

  //  if(!strncasecmp(p->buffer, "?xml", 4))
  if(*p->buffer == '?')
    {
    advance(p, 1);

    ret->pi = parse_tag_name(p);
    
    if(!skip_space(p))
      return 0;
    
    if(strncasecmp(p->buffer, "?>", 2))
      {
      if(!parse_attributes(p, &ret->attributes))
        return 0;
      }

    if(p->buffer_size < 2)
      {
      if(!more_data(p))
        return 0;
      }
    if(strncasecmp(p->buffer, "?>", 2))
      return 0;
    advance(p, 2);
    return 1;
    }
  
  ret->name = parse_tag_name(p);

  switch(*(p->buffer))
    {
    case ' ': /* Attributes might come */
      if(!skip_space(p))
        return 0;
      if(*(p->buffer) != '>')
        {
        if(!parse_attributes(p, &ret->attributes))
          return 0;
        
        switch(*(p->buffer))
          {
          case '>':
            advance(p, 1);
            break;
          case '/':
            advance(p, 1);
            if(!p->buffer_size)
              {
              if(!more_data(p))
                return 0;
              }
            if(*(p->buffer) == '>')
              {
              advance(p, 1);
              return 1;
              }
            else
              return 0;
            break;
          }
        }
      else
        {
        advance(p, 1);
        if(!parse_children(p, ret))
          return 0;
        }
      break;
    case '>': /* End of open tag */
      advance(p, 1);
      break;
    case '/': /* Node finished here? */
      advance(p, 1);
      if(!p->buffer_size)
        {
        if(!more_data(p))
          return 0;
        if(*(p->buffer) == '>')
          {
          advance(p, 1);
          return 1;
          }
        else
          return 0;
        }
      break;
    }
  /* Parse the children */

  if(!parse_children(p, ret))
    return 0;

  /* Now, we MUST have a closing tag */

  if((*p->buffer) != '<')
    return 0;
  advance(p, 1);
  
  if(!p->buffer_size)
    {
    if(!more_data(p))
      return 0;
    }
  if((*p->buffer) != '/')
    return 0;
  advance(p, 1);
  pos = find_chars(p, ">");
  if(!pos)
    return 0;
  if(strncasecmp(ret->name, p->buffer, (int)(pos - p->buffer)))
    return 0;
  advance(p, (int)(pos - p->buffer) + 1);
  return 1;
  }

static void skip_comment(parser_t * p)
  {
  char * pos;
  advance(p, 4);
  while(1)
    {
    pos = find_chars(p, "-");
    if(!pos)
      return;
    advance(p, (int)(pos - p->buffer));
    if(p->buffer_size < 3)
      {
      if(!more_data(p))
        return;
      }
    if((p->buffer[1] == '-') &&
       (p->buffer[2] == '>'))
      {
      advance(p, 3);
      return;
      }
    else
      advance(p, 1);
    }
  }

/* Parse one complete node, call this recursively for children */

static bgav_yml_node_t * parse_node(parser_t * p, int * is_comment)
  {
  bgav_yml_node_t * ret;
  
  *is_comment = 0;
  ret = calloc(1, sizeof(*ret));
  
  if(!p->buffer_size)
    {
    if(!more_data(p))
      goto fail;
    }

  if(*(p->buffer) == '<')
    {
    /* Skip comments */
    if(p->buffer_size < 4) 
      {
      if(!more_data(p))
        goto fail;
      }

    if((p->buffer[1] == '!') &&
       (p->buffer[2] == '-') &&
       (p->buffer[3] == '-'))
      {
      skip_comment(p);
      *is_comment = 1;
      goto fail;
      }
    
    if(!parse_tag_node(p, ret))
      goto fail;
    }
  else
    {
    if(!parse_text_node(p, ret))
      goto fail;
    }
  return ret;
  fail:
  bgav_yml_free(ret);
  return NULL;
  
  }

bgav_yml_node_t * bgav_yml_parse(bgav_input_context_t * input)
  {
  char c;
  parser_t parser;
  int is_comment = 1;
  bgav_yml_node_t * ret = NULL;
  bgav_yml_node_t * ret_end = NULL;
  
  memset(&parser, 0, sizeof(parser));
  parser.input = input;

  /* Skip leading spaces */

  while(1)
    {
    if(!bgav_input_get_data(input, (uint8_t*)(&c), 1))
      return NULL;
    if(isspace(c))
      bgav_input_skip(input, 1);
    else
      break;
    }
  while(1)
    {
    if(!ret)
      {
      ret = parse_node(&parser, &is_comment);

      if(ret)
        ret_end = ret;
      else if(!is_comment)
        break;
      }
    else
      {
      ret_end->next = parse_node(&parser, &is_comment);
      if(ret_end->next)
        ret_end = ret_end->next;
      else if(!is_comment)
        break;
      }
    }
  
  if(parser.buffer)
    free(parser.buffer);
  return ret;
  }

static void dump_attribute(bgav_yml_attr_t * a)
  {
  char * pos;

  bgav_dprintf( "%s=", a->name);
  
  pos = strchr(a->value, '"');

  if(pos)
    bgav_dprintf( "'%s'", a->value);
  else
    bgav_dprintf( "\"%s\"", a->value);
  
  }

static void dump_node(bgav_yml_node_t * n)
  {
  bgav_yml_attr_t * attr;
  bgav_yml_node_t * child;
  
  if(n->name || n->pi)
    {
    if(n->name)
      bgav_dprintf( "<%s", n->name);
    else if(n->pi)
      bgav_dprintf( "<?%s", n->pi);
    if(n->attributes)
      {
      bgav_dprintf( " ");
      attr = n->attributes;
      while(attr)
        {
        dump_attribute(attr);
        if(attr->next)
          bgav_dprintf( " ");
        attr = attr->next;
        }
      }
    if(!n->children)
      {
      if(n->name)
        bgav_dprintf( "/>");
      else if(n->pi)
        bgav_dprintf( "?>");
      }
    else
      {
      bgav_dprintf( ">");
      child = n->children;
      while(child)
        {
        dump_node(child);
        child = child->next;
        }
      bgav_dprintf( "</%s>", n->name);
      }

    }
  
  else if(n->str)
    bgav_dprintf( "%s", n->str);
  }

void bgav_yml_dump(bgav_yml_node_t * n)
  {
  while(n)
    {
    dump_node(n);
    bgav_dprintf( "\n");
    n = n->next;
    }
  }
  
#define FREE(p) if(p)free(p);

void bgav_yml_free(bgav_yml_node_t * n)
  {
  bgav_yml_node_t * tmp;
  bgav_yml_attr_t * attr;

  while(n)
    {
    tmp = n->next;
    
    bgav_yml_free(n->children);
    
    while(n->attributes)
      {
      attr = n->attributes->next;
      FREE(n->attributes->name);
      FREE(n->attributes->value);
      FREE(n->attributes);
      n->attributes = attr;
      }
    
    FREE(n->name);
    FREE(n->str);
    FREE(n->pi);
    
    free(n);
    n = tmp;
    }
  }

const char * bgav_yml_get_attribute(bgav_yml_node_t * n, const char * name)
  {
  bgav_yml_attr_t * attr;
  attr = n->attributes;
  while(attr)
    {
    if(attr->name && !strcasecmp(attr->name, name))
      return attr->value;
    attr = attr->next;
    }
  return NULL;
  }

const char * bgav_yml_get_attribute_i(bgav_yml_node_t * n, const char * name)
  {
  bgav_yml_attr_t * attr;
  attr = n->attributes;
  while(attr)
    {
    if(attr->name && !strcasecmp(attr->name, name))
      return attr->value;
    attr = attr->next;
    }
  return NULL;
  
  }

bgav_yml_node_t * bgav_yml_find_by_name(bgav_yml_node_t * yml, const char * name)
  {
  while(yml)
    {
    if(yml->name && !strcasecmp(yml->name, name))
      return yml;
    yml = yml->next;
    }
  return NULL;
  }

bgav_yml_node_t * bgav_yml_find_by_pi(bgav_yml_node_t * yml, const char * pi)
  {
  while(yml)
    {
    if(yml->pi && !strcasecmp(yml->pi, pi))
      return yml;
    yml = yml->next;
    }
  return NULL;
  
  }
