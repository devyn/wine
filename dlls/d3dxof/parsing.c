/*
 * X Files parsing
 *
 * Copyright 2008 Christian Costa
 *
 * This file contains the (internal) driver registration functions,
 * driver enumeration APIs and DirectDraw creation functions.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "wine/debug.h"

#define COBJMACROS

#include "winbase.h"
#include "wingdi.h"

#include "d3dxof_private.h"
#include "dxfile.h"

#include <stdio.h>

WINE_DEFAULT_DEBUG_CHANNEL(d3dxof);

#define TOKEN_NAME         1
#define TOKEN_STRING       2
#define TOKEN_INTEGER      3
#define TOKEN_GUID         5
#define TOKEN_INTEGER_LIST 6
#define TOKEN_FLOAT_LIST   7
#define TOKEN_OBRACE      10
#define TOKEN_CBRACE      11
#define TOKEN_OPAREN      12
#define TOKEN_CPAREN      13
#define TOKEN_OBRACKET    14
#define TOKEN_CBRACKET    15
#define TOKEN_OANGLE      16
#define TOKEN_CANGLE      17
#define TOKEN_DOT         18
#define TOKEN_COMMA       19
#define TOKEN_SEMICOLON   20
#define TOKEN_TEMPLATE    31
#define TOKEN_WORD        40
#define TOKEN_DWORD       41
#define TOKEN_FLOAT       42
#define TOKEN_DOUBLE      43
#define TOKEN_CHAR        44
#define TOKEN_UCHAR       45
#define TOKEN_SWORD       46
#define TOKEN_SDWORD      47
#define TOKEN_VOID        48
#define TOKEN_LPSTR       49
#define TOKEN_UNICODE     50
#define TOKEN_CSTRING     51
#define TOKEN_ARRAY       52

#define CLSIDFMT "<%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X>"

static const char* get_primitive_string(WORD token)
{
  switch(token)
  {
    case TOKEN_WORD:
      return "WORD";
    case TOKEN_DWORD:
      return "DWORD";
    case TOKEN_FLOAT:
      return "FLOAT";
    case TOKEN_DOUBLE:
      return "DOUBLE";
    case TOKEN_CHAR:
      return "CHAR";
    case TOKEN_UCHAR:
      return "UCHAR";
    case TOKEN_SWORD:
      return "SWORD";
    case TOKEN_SDWORD:
      return "SDWORD";
    case TOKEN_VOID:
      return "VOID";
    case TOKEN_LPSTR:
      return "STRING";
    case TOKEN_UNICODE:
      return "UNICODE";
    case TOKEN_CSTRING:
      return "CSTRING ";
    default:
      break;
  }
  return NULL;
}

void dump_template(xtemplate* templates_array, xtemplate* ptemplate)
{
  int j, k;
  GUID* clsid;

  clsid = &ptemplate->class_id;

  DPRINTF("template %s\n", ptemplate->name);
  DPRINTF("{\n");
  DPRINTF(CLSIDFMT "\n", clsid->Data1, clsid->Data2, clsid->Data3, clsid->Data4[0],
  clsid->Data4[1], clsid->Data4[2], clsid->Data4[3], clsid->Data4[4], clsid->Data4[5], clsid->Data4[6], clsid->Data4[7]);
  for (j = 0; j < ptemplate->nb_members; j++)
  {
    if (ptemplate->members[j].nb_dims)
      DPRINTF("array ");
    if (ptemplate->members[j].type == TOKEN_NAME)
      DPRINTF("%s ", templates_array[ptemplate->members[j].idx_template].name);
    else
      DPRINTF("%s ", get_primitive_string(ptemplate->members[j].type));
    DPRINTF("%s", ptemplate->members[j].name);
    for (k = 0; k < ptemplate->members[j].nb_dims; k++)
    {
      if (ptemplate->members[j].dim_fixed[k])
        DPRINTF("[%d]", ptemplate->members[j].dim_value[k]);
      else
        DPRINTF("[%s]", ptemplate->members[ptemplate->members[j].dim_value[k]].name);
    }
    DPRINTF(";\n");
  }
  if (ptemplate->open)
    DPRINTF("[...]\n");
  else if (ptemplate->nb_childs)
  {
    DPRINTF("[%s", ptemplate->childs[0]);
    for (j = 1; j < ptemplate->nb_childs; j++)
      DPRINTF(",%s", ptemplate->childs[j]);
    DPRINTF("]\n");
  }
  DPRINTF("}\n");
}

BOOL read_bytes(parse_buffer * buf, LPVOID data, DWORD size)
{
  if (buf->rem_bytes < size)
    return FALSE;
  memcpy(data, buf->buffer, size);
  buf->buffer += size;
  buf->rem_bytes -= size;
  return TRUE;
}

static void rewind_bytes(parse_buffer * buf, DWORD size)
{
  buf->buffer -= size;
  buf->rem_bytes += size;
}

static void dump_TOKEN(WORD token)
{
#define DUMP_TOKEN(t) case t: TRACE(#t "\n"); break
  switch(token)
  {
    DUMP_TOKEN(TOKEN_NAME);
    DUMP_TOKEN(TOKEN_STRING);
    DUMP_TOKEN(TOKEN_INTEGER);
    DUMP_TOKEN(TOKEN_GUID);
    DUMP_TOKEN(TOKEN_INTEGER_LIST);
    DUMP_TOKEN(TOKEN_FLOAT_LIST);
    DUMP_TOKEN(TOKEN_OBRACE);
    DUMP_TOKEN(TOKEN_CBRACE);
    DUMP_TOKEN(TOKEN_OPAREN);
    DUMP_TOKEN(TOKEN_CPAREN);
    DUMP_TOKEN(TOKEN_OBRACKET);
    DUMP_TOKEN(TOKEN_CBRACKET);
    DUMP_TOKEN(TOKEN_OANGLE);
    DUMP_TOKEN(TOKEN_CANGLE);
    DUMP_TOKEN(TOKEN_DOT);
    DUMP_TOKEN(TOKEN_COMMA);
    DUMP_TOKEN(TOKEN_SEMICOLON);
    DUMP_TOKEN(TOKEN_TEMPLATE);
    DUMP_TOKEN(TOKEN_WORD);
    DUMP_TOKEN(TOKEN_DWORD);
    DUMP_TOKEN(TOKEN_FLOAT);
    DUMP_TOKEN(TOKEN_DOUBLE);
    DUMP_TOKEN(TOKEN_CHAR);
    DUMP_TOKEN(TOKEN_UCHAR);
    DUMP_TOKEN(TOKEN_SWORD);
    DUMP_TOKEN(TOKEN_SDWORD);
    DUMP_TOKEN(TOKEN_VOID);
    DUMP_TOKEN(TOKEN_LPSTR);
    DUMP_TOKEN(TOKEN_UNICODE);
    DUMP_TOKEN(TOKEN_CSTRING);
    DUMP_TOKEN(TOKEN_ARRAY);
    default:
      if (0)
        TRACE("Unknown token %d\n", token);
      break;
  }
#undef DUMP_TOKEN
}

static BOOL is_space(char c)
{
  switch (c)
  {
    case 0x00:
    case 0x0D:
    case 0x0A:
    case ' ':
    case '\t':
      return TRUE;
  }
  return FALSE;
}

static BOOL is_operator(char c)
{
  switch(c)
  {
    case '{':
    case '}':
    case '[':
    case ']':
    case '(':
    case ')':
    case '<':
    case '>':
    case ',':
    case ';':
      return TRUE;
  }
  return FALSE;
}

static inline BOOL is_separator(char c)
{
  return is_space(c) || is_operator(c);
}

static WORD get_operator_token(char c)
{
  switch(c)
  {
    case '{':
      return TOKEN_OBRACE;
    case '}':
      return TOKEN_CBRACE;
    case '[':
      return TOKEN_OBRACKET;
    case ']':
      return TOKEN_CBRACKET;
    case '(':
      return TOKEN_OPAREN;
    case ')':
      return TOKEN_CPAREN;
    case '<':
      return TOKEN_OANGLE;
    case '>':
      return TOKEN_CANGLE;
    case ',':
      return TOKEN_COMMA;
    case ';':
      return TOKEN_SEMICOLON;
  }
  return 0;
}

static BOOL is_keyword(parse_buffer* buf, const char* keyword)
{
  char tmp[9]; /* template keyword size + 1 */
  DWORD len = strlen(keyword);
  read_bytes(buf, tmp, len+1);
  if (!strncasecmp(tmp, keyword,len) && is_separator(tmp[len]))
  {
    rewind_bytes(buf, 1);
    return TRUE;
  }
  rewind_bytes(buf, len+1);
  return FALSE;
}

static WORD get_keyword_token(parse_buffer* buf)
{
  if (is_keyword(buf, "template"))
    return TOKEN_TEMPLATE;
  if (is_keyword(buf, "WORD"))
    return TOKEN_WORD;
  if (is_keyword(buf, "DWORD"))
    return TOKEN_DWORD;
  if (is_keyword(buf, "FLOAT"))
    return TOKEN_FLOAT;
  if (is_keyword(buf, "DOUBLE"))
    return TOKEN_DOUBLE;
  if (is_keyword(buf, "CHAR"))
    return TOKEN_CHAR;
  if (is_keyword(buf, "UCHAR"))
    return TOKEN_UCHAR;
  if (is_keyword(buf, "SWORD"))
    return TOKEN_SWORD;
  if (is_keyword(buf, "SDWORD"))
    return TOKEN_SDWORD;
  if (is_keyword(buf, "VOID"))
    return TOKEN_VOID;
  if (is_keyword(buf, "STRING"))
    return TOKEN_LPSTR;
  if (is_keyword(buf, "UNICODE"))
    return TOKEN_UNICODE;
  if (is_keyword(buf, "CSTRING"))
    return TOKEN_CSTRING;
  if (is_keyword(buf, "array"))
    return TOKEN_ARRAY;

  return 0;
}

static BOOL is_guid(parse_buffer* buf)
{
  char tmp[50];
  DWORD pos = 1;
  GUID class_id;
  DWORD tab[10];
  int ret;

  if (*buf->buffer != '<')
    return FALSE;
  tmp[0] = '<';
  while (*(buf->buffer+pos) != '>')
  {
    tmp[pos] = *(buf->buffer+pos);
    pos++;
  }
  tmp[pos++] = '>';
  tmp[pos] = 0;
  if (pos != 38 /* <+36+> */)
  {
    TRACE("Wrong guid %s (%d)\n", tmp, pos);
    return FALSE;
  }
  buf->buffer += pos;
  buf->rem_bytes -= pos;

  ret = sscanf(tmp, CLSIDFMT, &class_id.Data1, tab, tab+1, tab+2, tab+3, tab+4, tab+5, tab+6, tab+7, tab+8, tab+9);
  if (ret != 11)
  {
    TRACE("Wrong guid %s (%d)\n", tmp, pos);
    return FALSE;
  }
  TRACE("Found guid %s (%d)\n", tmp, pos);

  class_id.Data2 = tab[0];
  class_id.Data3 = tab[1];
  class_id.Data4[0] = tab[2];
  class_id.Data4[1] = tab[3];
  class_id.Data4[2] = tab[4];
  class_id.Data4[3] = tab[5];
  class_id.Data4[4] = tab[6];
  class_id.Data4[5] = tab[7];
  class_id.Data4[6] = tab[8];
  class_id.Data4[7] = tab[9];

  *(GUID*)buf->value = class_id;

  return TRUE;
}

static BOOL is_name(parse_buffer* buf)
{
  char tmp[50];
  DWORD pos = 0;
  char c;
  BOOL error = 0;
  while (!is_separator(c = *(buf->buffer+pos)))
  {
    if (!(((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '_') || (c == '-')))
      error = 1;
    tmp[pos++] = c;
  }
  tmp[pos] = 0;

  if (error)
  {
    TRACE("Wrong name %s\n", tmp);
    return FALSE;
  }

  buf->buffer += pos;
  buf->rem_bytes -= pos;

  TRACE("Found name %s\n", tmp);
  strcpy((char*)buf->value, tmp);

  return TRUE;
}

static BOOL is_float(parse_buffer* buf)
{
  char tmp[50];
  DWORD pos = 0;
  char c;
  float decimal;
  BOOL dot = 0;

  while (!is_separator(c = *(buf->buffer+pos)))
  {
    if (!((!pos && (c == '-')) || ((c >= '0') && (c <= '9')) || (!dot && (c == '.'))))
      return FALSE;
    if (c == '.')
      dot = TRUE;
    tmp[pos++] = c;
  }
  tmp[pos] = 0;

  buf->buffer += pos;
  buf->rem_bytes -= pos;

  sscanf(tmp, "%f", &decimal);

  TRACE("Found float %s - %f\n", tmp, decimal);

  *(float*)buf->value = decimal;

  return TRUE;
}

static BOOL is_integer(parse_buffer* buf)
{
  char tmp[50];
  DWORD pos = 0;
  char c;
  DWORD integer;

  while (!is_separator(c = *(buf->buffer+pos)))
  {
    if (!((c >= '0') && (c <= '9')))
      return FALSE;
    tmp[pos++] = c;
  }
  tmp[pos] = 0;

  buf->buffer += pos;
  buf->rem_bytes -= pos;

  sscanf(tmp, "%d", &integer);

  TRACE("Found integer %s - %d\n", tmp, integer);

  *(DWORD*)buf->value = integer;

  return TRUE;
}

static BOOL is_string(parse_buffer* buf)
{
  char tmp[32];
  DWORD pos = 0;
  char c;
  BOOL ok = 0;

  if (*buf->buffer != '"')
    return FALSE;

  while (!is_separator(c = *(buf->buffer+pos+1)) && (pos < 31))
  {
    if (c == '"')
    {
      ok = 1;
      break;
    }
    tmp[pos++] = c;
  }
  tmp[pos] = 0;

  if (!ok)
  {
    TRACE("Wrong string %s\n", tmp);
    return FALSE;
  }

  buf->buffer += pos + 2;
  buf->rem_bytes -= pos + 2;

  TRACE("Found string %s\n", tmp);
  strcpy((char*)buf->value, tmp);

  return TRUE;
}

static WORD parse_TOKEN(parse_buffer * buf)
{
  WORD token;

  if (buf->txt)
  {
    while(1)
    {
      char c;
      if (!read_bytes(buf, &c, 1))
        return 0;
      /*TRACE("char = '%c'\n", is_space(c) ? ' ' : c);*/
      if ((c == '#') || (c == '/'))
      {
        /* Handle comment (# or //) */
        if (c == '/')
        {
          if (!read_bytes(buf, &c, 1))
            return 0;
          if (c != '/')
            return 0;
        }
        c = 0;
        while (c != 0x0A)
        {
          if (!read_bytes(buf, &c, 1))
            return 0;
        }
        continue;
      }
      if (is_space(c))
        continue;
      if (is_operator(c) && (c != '<'))
      {
        token = get_operator_token(c);
        break;
      }
      else if (c == '.')
      {
        token = TOKEN_DOT;
        break;
      }
      else
      {
        rewind_bytes(buf, 1);

        if ((token = get_keyword_token(buf)))
          break;

        if (is_guid(buf))
        {
          token = TOKEN_GUID;
          break;
        }
        if (is_integer(buf))
        {
          token = TOKEN_INTEGER;
          break;
        }
        if (is_float(buf))
        {
          token = TOKEN_FLOAT;
          break;
        }
        if (is_string(buf))
        {
          token = TOKEN_LPSTR;
          break;
        }
        if (is_name(buf))
        {
          token = TOKEN_NAME;
          break;
        }

        FIXME("Unrecognize element\n");
        return 0;
      }
    }
  }
  else
  {
    static int nb_elem;
    static int is_float;

    if (!nb_elem)
    {
      if (!read_bytes(buf, &token, 2))
        return 0;

      /* Convert integer and float list into separate elements */
      if (token == TOKEN_INTEGER_LIST)
      {
        if (!read_bytes(buf, &nb_elem, 4))
          return 0;
        token = TOKEN_INTEGER;
        is_float = FALSE;
        TRACE("Integer list (TOKEN_INTEGER_LIST) of size %d\n", nb_elem);
      }
      else if (token == TOKEN_FLOAT_LIST)
      {
        if (!read_bytes(buf, &nb_elem, 4))
          return 0;
        token = TOKEN_FLOAT;
        is_float = TRUE;
        TRACE("Float list (TOKEN_FLOAT_LIST) of size %d\n", nb_elem);
      }
    }

    if (nb_elem)
    {
      token = is_float ? TOKEN_FLOAT : TOKEN_INTEGER;
      nb_elem--;
        {
          DWORD integer;

          if (!read_bytes(buf, &integer, 4))
            return 0;

          *(DWORD*)buf->value = integer;
        }
      dump_TOKEN(token);
      return token;
    }

    switch (token)
    {
      case TOKEN_NAME:
        {
          DWORD count;
          char strname[100];

          if (!read_bytes(buf, &count, 4))
            return 0;
          if (!read_bytes(buf, strname, count))
            return 0;
          strname[count] = 0;
          /*TRACE("name = %s\n", strname);*/

          strcpy((char*)buf->value, strname);
        }
        break;
      case TOKEN_INTEGER:
        {
          DWORD integer;

          if (!read_bytes(buf, &integer, 4))
            return 0;
          /*TRACE("integer = %ld\n", integer);*/

          *(DWORD*)buf->value = integer;
        }
        break;
      case TOKEN_GUID:
        {
          char strguid[39];
          GUID class_id;

          if (!read_bytes(buf, &class_id, 16))
            return 0;
          sprintf(strguid, CLSIDFMT, class_id.Data1, class_id.Data2, class_id.Data3, class_id.Data4[0],
            class_id.Data4[1], class_id.Data4[2], class_id.Data4[3], class_id.Data4[4], class_id.Data4[5],
            class_id.Data4[6], class_id.Data4[7]);
          /*TRACE("guid = {%s}\n", strguid);*/

          *(GUID*)buf->value = class_id;
        }
        break;
      case TOKEN_STRING:
        {
          DWORD count;
          WORD tmp_token;
          char strname[100];
          if (!read_bytes(buf, &count, 4))
            return 0;
          if (!read_bytes(buf, strname, count))
            return 0;
          strname[count] = 0;
          if (!read_bytes(buf, &tmp_token, 2))
            return 0;
          if ((tmp_token != TOKEN_COMMA) && (tmp_token != TOKEN_SEMICOLON))
            ERR("No comma or semicolon (got %d)\n", tmp_token);
          /*TRACE("name = %s\n", strname);*/

          strcpy((char*)buf->value, strname);
          token = TOKEN_LPSTR;
        }
        break;
      case TOKEN_OBRACE:
      case TOKEN_CBRACE:
      case TOKEN_OPAREN:
      case TOKEN_CPAREN:
      case TOKEN_OBRACKET:
      case TOKEN_CBRACKET:
      case TOKEN_OANGLE:
      case TOKEN_CANGLE:
      case TOKEN_DOT:
      case TOKEN_COMMA:
      case TOKEN_SEMICOLON:
      case TOKEN_TEMPLATE:
      case TOKEN_WORD:
      case TOKEN_DWORD:
      case TOKEN_FLOAT:
      case TOKEN_DOUBLE:
      case TOKEN_CHAR:
      case TOKEN_UCHAR:
      case TOKEN_SWORD:
      case TOKEN_SDWORD:
      case TOKEN_VOID:
      case TOKEN_LPSTR:
      case TOKEN_UNICODE:
      case TOKEN_CSTRING:
      case TOKEN_ARRAY:
        break;
      default:
        return 0;
    }
  }

  dump_TOKEN(token);

  return token;
}

static WORD get_TOKEN(parse_buffer * buf)
{
  if (buf->token_present)
  {
    buf->token_present = FALSE;
    return buf->current_token;
  }

  buf->current_token = parse_TOKEN(buf);

  return buf->current_token;
}

static WORD check_TOKEN(parse_buffer * buf)
{
  if (buf->token_present)
    return buf->current_token;

  buf->current_token = parse_TOKEN(buf);
  buf->token_present = TRUE;

  return buf->current_token;
}

BOOL is_template_available(parse_buffer * buf)
{
  return check_TOKEN(buf) == TOKEN_TEMPLATE;
}

static inline BOOL is_primitive_type(WORD token)
{
  BOOL ret;
  switch(token)
  {
    case TOKEN_WORD:
    case TOKEN_DWORD:
    case TOKEN_FLOAT:
    case TOKEN_DOUBLE:
    case TOKEN_CHAR:
    case TOKEN_UCHAR:
    case TOKEN_SWORD:
    case TOKEN_SDWORD:
    case TOKEN_LPSTR:
    case TOKEN_UNICODE:
    case TOKEN_CSTRING:
      ret = 1;
      break;
    default:
      ret = 0;
      break;
  }
  return ret;
}

static BOOL parse_template_option_info(parse_buffer * buf)
{
  xtemplate* cur_template = &buf->pdxf->xtemplates[buf->pdxf->nb_xtemplates];

  if (check_TOKEN(buf) == TOKEN_DOT)
  {
    get_TOKEN(buf);
    if (get_TOKEN(buf) != TOKEN_DOT)
      return FALSE;
    if (get_TOKEN(buf) != TOKEN_DOT)
      return FALSE;
    cur_template->open = TRUE;
  }
  else
  {
    while (1)
    {
      if (get_TOKEN(buf) != TOKEN_NAME)
        return FALSE;
      strcpy(cur_template->childs[cur_template->nb_childs], (char*)buf->value);
      if (check_TOKEN(buf) == TOKEN_GUID)
        get_TOKEN(buf);
      cur_template->nb_childs++;
      if (check_TOKEN(buf) != TOKEN_COMMA)
        break;
      get_TOKEN(buf);
    }
    cur_template->open = FALSE;
  }

  return TRUE;
}

static BOOL parse_template_members_list(parse_buffer * buf)
{
  int idx_member = 0;
  member* cur_member;

  while (1)
  {
    BOOL array = 0;
    int nb_dims = 0;
    cur_member = &buf->pdxf->xtemplates[buf->pdxf->nb_xtemplates].members[idx_member];

    if (check_TOKEN(buf) == TOKEN_ARRAY)
    {
      get_TOKEN(buf);
      array = 1;
    }

    if (check_TOKEN(buf) == TOKEN_NAME)
    {
      cur_member->type = get_TOKEN(buf);
      cur_member->idx_template = 0;
      while (cur_member->idx_template < buf->pdxf->nb_xtemplates)
      {
        if (!strcasecmp((char*)buf->value, buf->pdxf->xtemplates[cur_member->idx_template].name))
          break;
        cur_member->idx_template++;
      }
      if (cur_member->idx_template == buf->pdxf->nb_xtemplates)
      {
        ERR("Reference to a nonexistent template '%s'\n", (char*)buf->value);
        return FALSE;
      }
    }
    else if (is_primitive_type(check_TOKEN(buf)))
      cur_member->type = get_TOKEN(buf);
    else
      break;

    if (get_TOKEN(buf) != TOKEN_NAME)
      return FALSE;
    strcpy(cur_member->name, (char*)buf->value);

    if (array)
    {
      while (check_TOKEN(buf) == TOKEN_OBRACKET)
      {
        if (nb_dims >= MAX_ARRAY_DIM)
        {
          FIXME("Too many dimensions (%d) for multi-dimensional array\n", nb_dims + 1);
          return FALSE;
        }
        get_TOKEN(buf);
        if (check_TOKEN(buf) == TOKEN_INTEGER)
        {
          get_TOKEN(buf);
          cur_member->dim_fixed[nb_dims] = TRUE;
          cur_member->dim_value[nb_dims] = *(DWORD*)buf->value;
        }
        else
        {
          int i;
          if (get_TOKEN(buf) != TOKEN_NAME)
            return FALSE;
          for (i = 0; i < idx_member; i++)
          {
            if (!strcmp((char*)buf->value, buf->pdxf->xtemplates[buf->pdxf->nb_xtemplates].members[i].name))
            {
              if (buf->pdxf->xtemplates[buf->pdxf->nb_xtemplates].members[i].nb_dims)
              {
                ERR("Array cannot be used to specify variable array size\n");
                return FALSE;
              }
              if (buf->pdxf->xtemplates[buf->pdxf->nb_xtemplates].members[i].type != TOKEN_DWORD)
              {
                FIXME("Only DWORD supported to specify variable array size\n");
                return FALSE;
              }
              break;
            }
          }
          if (i == idx_member)
          {
            ERR("Reference to unknown member %s\n", (char*)buf->value);
            return FALSE;
          }
          cur_member->dim_fixed[nb_dims] = FALSE;
          cur_member->dim_value[nb_dims] = i;
        }
        if (get_TOKEN(buf) != TOKEN_CBRACKET)
          return FALSE;
        nb_dims++;
      }
      if (!nb_dims)
        return FALSE;
      cur_member->nb_dims = nb_dims;
    }
    if (get_TOKEN(buf) != TOKEN_SEMICOLON)
      return FALSE;

    idx_member++;
  }

  buf->pdxf->xtemplates[buf->pdxf->nb_xtemplates].nb_members = idx_member;

  return TRUE;
}

static BOOL parse_template_parts(parse_buffer * buf)
{
  if (!parse_template_members_list(buf))
    return FALSE;
  if (check_TOKEN(buf) == TOKEN_OBRACKET)
  {
    get_TOKEN(buf);
    if (!parse_template_option_info(buf))
      return FALSE;
    if (get_TOKEN(buf) != TOKEN_CBRACKET)
     return FALSE;
  }

  return TRUE;
}

static void go_to_next_definition(parse_buffer * buf)
{
  char c;
  while (buf->rem_bytes)
  {
    read_bytes(buf, &c, 1);
    if ((c == '#') || (c == '/'))
    {
      /* Handle comment (# or //) */
      if (c == '/')
      {
        if (!read_bytes(buf, &c, 1))
          return;
        if (c != '/')
          return;
      }
      c = 0;
      while (c != 0x0A)
      {
        if (!read_bytes(buf, &c, 1))
          return;
      }
      continue;
    }
    else if (!is_space(c))
    {
      rewind_bytes(buf, 1);
      break;
    }
  }
}

BOOL parse_template(parse_buffer * buf)
{
  if (get_TOKEN(buf) != TOKEN_TEMPLATE)
    return FALSE;
  if (get_TOKEN(buf) != TOKEN_NAME)
    return FALSE;
  strcpy(buf->pdxf->xtemplates[buf->pdxf->nb_xtemplates].name, (char*)buf->value);
  if (get_TOKEN(buf) != TOKEN_OBRACE)
    return FALSE;
  if (get_TOKEN(buf) != TOKEN_GUID)
    return FALSE;
  buf->pdxf->xtemplates[buf->pdxf->nb_xtemplates].class_id = *(GUID*)buf->value;
  if (!parse_template_parts(buf))
    return FALSE;
  if (get_TOKEN(buf) != TOKEN_CBRACE)
    return FALSE;
  if (buf->txt)
  {
    /* Go to the next template */
    go_to_next_definition(buf);
  }

  TRACE("%d - %s - %s\n", buf->pdxf->nb_xtemplates, buf->pdxf->xtemplates[buf->pdxf->nb_xtemplates].name, debugstr_guid(&buf->pdxf->xtemplates[buf->pdxf->nb_xtemplates].class_id));
  buf->pdxf->nb_xtemplates++;

  return TRUE;
}

static BOOL parse_object_parts(parse_buffer * buf, BOOL allow_optional);
static BOOL parse_object_members_list(parse_buffer * buf)
{
  DWORD token;
  int i;
  xtemplate* pt = buf->pxt[buf->level];
  DWORD last_dword = 0;

  for (i = 0; i < pt->nb_members; i++)
  {
    int k;
    int nb_elems = 1;

    buf->pxo->members[i].name = pt->members[i].name;
    buf->pxo->members[i].start = buf->cur_pdata;

    for (k = 0; k < pt->members[i].nb_dims; k++)
    {
      if (pt->members[i].dim_fixed[k])
        nb_elems *= pt->members[i].dim_value[k];
      else
        nb_elems *= *(DWORD*)buf->pxo->members[pt->members[i].dim_value[k]].start;
    }

    TRACE("Elements to consider: %d\n", nb_elems);

    for (k = 0; k < nb_elems; k++)
    {
      if (buf->txt && k)
      {
        token = check_TOKEN(buf);
        if (token == TOKEN_COMMA)
        {
          get_TOKEN(buf);
        }
        else
        {
          /* Allow comma omission */
          if (!((token == TOKEN_FLOAT) || (token == TOKEN_INTEGER)))
            return FALSE;
        }
      }

      if (pt->members[i].type == TOKEN_NAME)
      {
        int j;

        TRACE("Found sub-object %s\n", buf->pdxf->xtemplates[pt->members[i].idx_template].name);
        buf->level++;
        /* To do template lookup */
        for (j = 0; j < buf->pdxf->nb_xtemplates; j++)
        {
          if (!strcasecmp(buf->pdxf->xtemplates[pt->members[i].idx_template].name, buf->pdxf->xtemplates[j].name))
          {
            buf->pxt[buf->level] = &buf->pdxf->xtemplates[j];
            break;
          }
        }
        if (j == buf->pdxf->nb_xtemplates)
        {
          ERR("Unknown template %s\n", (char*)buf->value);
          buf->level--;
          return FALSE;
        }
        TRACE("Enter %s\n", buf->pdxf->xtemplates[pt->members[i].idx_template].name);
        if (!parse_object_parts(buf, FALSE))
        {
          buf->level--;
          return FALSE;
        }
        buf->level--;
      }
      else
      {
        token = check_TOKEN(buf);
        if (token == TOKEN_INTEGER)
        {
          get_TOKEN(buf);
          last_dword = *(DWORD*)buf->value;
          TRACE("%s = %d\n", pt->members[i].name, *(DWORD*)buf->value);
          /* Assume larger size */
          if ((buf->cur_pdata - buf->pdata + 4) > MAX_DATA_SIZE)
          {
            FIXME("Buffer too small\n");
            return FALSE;
          }
          if (pt->members[i].type == TOKEN_WORD)
          {
            *(((WORD*)(buf->cur_pdata))) = (WORD)(*(DWORD*)buf->value);
            buf->cur_pdata += 2;
          }
          else if (pt->members[i].type == TOKEN_DWORD)
          {
            *(((DWORD*)(buf->cur_pdata))) = (DWORD)(*(DWORD*)buf->value);
            buf->cur_pdata += 4;
          }
          else
          {
            FIXME("Token %d not supported\n", pt->members[i].type);
            return FALSE;
          }
        }
        else if (token == TOKEN_FLOAT)
        {
          get_TOKEN(buf);
          TRACE("%s = %f\n", pt->members[i].name, *(float*)buf->value);
          /* Assume larger size */
          if ((buf->cur_pdata - buf->pdata + 4) > MAX_DATA_SIZE)
          {
            FIXME("Buffer too small\n");
            return FALSE;
          }
          if (pt->members[i].type == TOKEN_FLOAT)
          {
            *(((float*)(buf->cur_pdata))) = (float)(*(float*)buf->value);
            buf->cur_pdata += 4;
          }
          else
          {
            FIXME("Token %d not supported\n", pt->members[i].type);
            return FALSE;
          }
        }
        else if (token == TOKEN_LPSTR)
        {
          get_TOKEN(buf);
          TRACE("%s = %s\n", pt->members[i].name, (char*)buf->value);
          /* Assume larger size */
          if ((buf->cur_pdata - buf->pdata + 4) > MAX_DATA_SIZE)
          {
            FIXME("Buffer too small\n");
            return FALSE;
          }
          if (pt->members[i].type == TOKEN_LPSTR)
          {
            int len = strlen((char*)buf->value) + 1;
            if ((buf->cur_pstrings - buf->pstrings + len) > MAX_STRINGS_BUFFER)
            {
              FIXME("Buffer too small %p %p %d\n", buf->cur_pstrings, buf->pstrings, len);
              return FALSE;
            }
            strcpy((char*)buf->cur_pstrings, (char*)buf->value);
            *(((LPCSTR*)(buf->cur_pdata))) = (char*)buf->cur_pstrings;
            buf->cur_pstrings += len;
            buf->cur_pdata += 4;
          }
          else
          {
            FIXME("Token %d not supported\n", pt->members[i].type);
            return FALSE;
          }
        }
        else
	{
          FIXME("Unexpected token %d\n", token);
          return FALSE;
        }
      }
    }

    if (buf->txt && (check_TOKEN(buf) != TOKEN_CBRACE))
    {
      token = get_TOKEN(buf);
      if ((token != TOKEN_SEMICOLON) && (token != TOKEN_COMMA))
      {
        /* Allow comma instead of semicolon in some specific cases */
        if (!((token == TOKEN_COMMA) && ((i+1) < pt->nb_members) && (pt->members[i].type == pt->members[i+1].type)
          && (!pt->members[i].nb_dims) && (!pt->members[i+1].nb_dims)))
          return FALSE;
      }
    }
  }

  return TRUE;
}

static BOOL parse_object_parts(parse_buffer * buf, BOOL allow_optional)
{
  if (!parse_object_members_list(buf))
    return FALSE;

  if (allow_optional)
  {
    buf->pxo->size = buf->cur_pdata - buf->pxo->pdata;

    /* Skip trailing semicolon */
    while (check_TOKEN(buf) == TOKEN_SEMICOLON)
      get_TOKEN(buf);

    while (1)
    {
      if (check_TOKEN(buf) == TOKEN_OBRACE)
      {
        int i, j;
        get_TOKEN(buf);
        if (get_TOKEN(buf) != TOKEN_NAME)
          return FALSE;
        if (get_TOKEN(buf) != TOKEN_CBRACE)
          return FALSE;
        TRACE("Found optional reference %s\n", (char*)buf->value);
        for (i = 0; i < buf->nb_pxo_globals; i++)
        {
          for (j = 0; j < (buf->pxo_globals[i])[0].nb_subobjects; j++)
          {
            if (!strcmp((buf->pxo_globals[i])[j].name, (char*)buf->value))
              goto _exit;
          }
        }
_exit:
        if (i == buf->nb_pxo_globals)
        {
          ERR("Reference to unknown object %s\n", (char*)buf->value);
          return FALSE;
        }
        buf->pxo->childs[buf->pxo->nb_childs] = &buf->pxo_tab[buf->cur_subobject++];
        buf->pxo->childs[buf->pxo->nb_childs]->ptarget = &(buf->pxo_globals[i])[j];
        buf->pxo->nb_childs++;
      }
      else if (check_TOKEN(buf) == TOKEN_NAME)
      {
        xobject* pxo = buf->pxo;
        buf->pxo = buf->pxo->childs[buf->pxo->nb_childs] = &buf->pxo_tab[buf->cur_subobject++];

        TRACE("Enter optional %s\n", (char*)buf->value);
        buf->level++;
        if (!parse_object(buf))
        {
          buf->level--;
          return FALSE;
        }
        buf->level--;
        buf->pxo = pxo;
        buf->pxo->nb_childs++;
      }
      else
        break;
    }
  }

  if (buf->pxo->nb_childs > MAX_CHILDS)
  {
    FIXME("Too many childs %d\n", buf->pxo->nb_childs);
    return FALSE;
  }

  return TRUE;
}

BOOL parse_object(parse_buffer * buf)
{
  int i;

  buf->pxo->pdata = buf->cur_pdata;
  buf->pxo->ptarget = NULL;

  if (get_TOKEN(buf) != TOKEN_NAME)
    return FALSE;

  /* To do template lookup */
  for (i = 0; i < buf->pdxf->nb_xtemplates; i++)
  {
    if (!strcasecmp((char*)buf->value, buf->pdxf->xtemplates[i].name))
    {
      buf->pxt[buf->level] = &buf->pdxf->xtemplates[i];
      memcpy(&buf->pxo->type, &buf->pdxf->xtemplates[i].class_id, 16);
      break;
    }
  }
  if (i == buf->pdxf->nb_xtemplates)
  {
    ERR("Unknown template %s\n", (char*)buf->value);
    return FALSE;
  }

  if (check_TOKEN(buf) == TOKEN_NAME)
  {
    get_TOKEN(buf);
    strcpy(buf->pxo->name, (char*)buf->value);
  }
  else
    buf->pxo->name[0] = 0;

  if (get_TOKEN(buf) != TOKEN_OBRACE)
    return FALSE;
  if (check_TOKEN(buf) == TOKEN_GUID)
  {
    get_TOKEN(buf);
    memcpy(&buf->pxo->class_id, buf->value, 16);
  }
  else
    memset(&buf->pxo->class_id, 0, 16);

  if (!parse_object_parts(buf, TRUE))
    return FALSE;
  if (get_TOKEN(buf) != TOKEN_CBRACE)
    return FALSE;

  if (buf->txt)
  {
    /* Go to the next object */
    go_to_next_definition(buf);
  }

  return TRUE;
}
