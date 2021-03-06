/*
 * Format String Generator for IDL Compiler
 *
 * Copyright 2005-2006 Eric Kohl
 * Copyright 2005-2006 Robert Shearman
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
#include "wine/port.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <limits.h>

#include "widl.h"
#include "utils.h"
#include "parser.h"
#include "header.h"
#include "typetree.h"

#include "typegen.h"
#include "expr.h"

/* round size up to multiple of alignment */
#define ROUND_SIZE(size, alignment) (((size) + ((alignment) - 1)) & ~((alignment) - 1))
/* value to add on to round size up to a multiple of alignment */
#define ROUNDING(size, alignment) (((alignment) - 1) - (((size) + ((alignment) - 1)) & ((alignment) - 1)))

static const var_t *current_func;
static const type_t *current_structure;
static const type_t *current_iface;

static struct list expr_eval_routines = LIST_INIT(expr_eval_routines);
struct expr_eval_routine
{
    struct list entry;
    const type_t *structure;
    unsigned int baseoff;
    const expr_t *expr;
};

static size_t fields_memsize(const var_list_t *fields, unsigned int *align);
static size_t write_struct_tfs(FILE *file, type_t *type, const char *name, unsigned int *tfsoff);
static int write_embedded_types(FILE *file, const attr_list_t *attrs, type_t *type,
                                const char *name, int write_ptr, unsigned int *tfsoff);
static const var_t *find_array_or_string_in_struct(const type_t *type);
static size_t write_string_tfs(FILE *file, const attr_list_t *attrs,
                               type_t *type,
                               const char *name, unsigned int *typestring_offset);

const char *string_of_type(unsigned char type)
{
    switch (type)
    {
    case RPC_FC_BYTE: return "FC_BYTE";
    case RPC_FC_CHAR: return "FC_CHAR";
    case RPC_FC_SMALL: return "FC_SMALL";
    case RPC_FC_USMALL: return "FC_USMALL";
    case RPC_FC_WCHAR: return "FC_WCHAR";
    case RPC_FC_SHORT: return "FC_SHORT";
    case RPC_FC_USHORT: return "FC_USHORT";
    case RPC_FC_LONG: return "FC_LONG";
    case RPC_FC_ULONG: return "FC_ULONG";
    case RPC_FC_FLOAT: return "FC_FLOAT";
    case RPC_FC_HYPER: return "FC_HYPER";
    case RPC_FC_DOUBLE: return "FC_DOUBLE";
    case RPC_FC_ENUM16: return "FC_ENUM16";
    case RPC_FC_ENUM32: return "FC_ENUM32";
    case RPC_FC_IGNORE: return "FC_IGNORE";
    case RPC_FC_ERROR_STATUS_T: return "FC_ERROR_STATUS_T";
    case RPC_FC_RP: return "FC_RP";
    case RPC_FC_UP: return "FC_UP";
    case RPC_FC_OP: return "FC_OP";
    case RPC_FC_FP: return "FC_FP";
    case RPC_FC_ENCAPSULATED_UNION: return "FC_ENCAPSULATED_UNION";
    case RPC_FC_NON_ENCAPSULATED_UNION: return "FC_NON_ENCAPSULATED_UNION";
    case RPC_FC_STRUCT: return "FC_STRUCT";
    case RPC_FC_PSTRUCT: return "FC_PSTRUCT";
    case RPC_FC_CSTRUCT: return "FC_CSTRUCT";
    case RPC_FC_CPSTRUCT: return "FC_CPSTRUCT";
    case RPC_FC_CVSTRUCT: return "FC_CVSTRUCT";
    case RPC_FC_BOGUS_STRUCT: return "FC_BOGUS_STRUCT";
    case RPC_FC_SMFARRAY: return "FC_SMFARRAY";
    case RPC_FC_LGFARRAY: return "FC_LGFARRAY";
    case RPC_FC_SMVARRAY: return "FC_SMVARRAY";
    case RPC_FC_LGVARRAY: return "FC_LGVARRAY";
    case RPC_FC_CARRAY: return "FC_CARRAY";
    case RPC_FC_CVARRAY: return "FC_CVARRAY";
    case RPC_FC_BOGUS_ARRAY: return "FC_BOGUS_ARRAY";
    case RPC_FC_ALIGNM4: return "FC_ALIGNM4";
    case RPC_FC_ALIGNM8: return "FC_ALIGNM8";
    case RPC_FC_POINTER: return "FC_POINTER";
    case RPC_FC_C_CSTRING: return "FC_C_CSTRING";
    case RPC_FC_C_WSTRING: return "FC_C_WSTRING";
    case RPC_FC_CSTRING: return "FC_CSTRING";
    case RPC_FC_WSTRING: return "FC_WSTRING";
    default:
        error("string_of_type: unknown type 0x%02x\n", type);
        return NULL;
    }
}

static unsigned char get_pointer_fc(const type_t *type)
{
    assert(is_ptr(type));
    /* FIXME: see corresponding hack in set_type - we shouldn't be getting
     * the pointer type from an alias, rather determining it from the
     * position */
    return type->type;
}

static int get_struct_type(const type_t *type)
{
  int has_pointer = 0;
  int has_conformance = 0;
  int has_variance = 0;
  var_t *field;
  var_list_t *fields;

  if (type->type != RPC_FC_STRUCT) return type->type;

  fields = type_struct_get_fields(type);

  if (get_padding(fields))
    return RPC_FC_BOGUS_STRUCT;

  if (fields) LIST_FOR_EACH_ENTRY( field, fields, var_t, entry )
  {
    type_t *t = field->type;

    if (is_user_type(t))
      return RPC_FC_BOGUS_STRUCT;

    if (field->type->declarray)
    {
        if (is_string_type(field->attrs, field->type))
        {
            if (is_conformant_array(field->type))
                has_conformance = 1;
            has_variance = 1;
            continue;
        }

        if (is_array(type_array_get_element(field->type)))
            return RPC_FC_BOGUS_STRUCT;

        if (type_array_has_conformance(field->type))
        {
            has_conformance = 1;
            if (field->type->declarray && list_next(fields, &field->entry))
                error_loc("field '%s' deriving from a conformant array must be the last field in the structure\n",
                        field->name);
        }
        if (type_array_has_variance(field->type))
            has_variance = 1;

        t = type_array_get_element(field->type);
    }

    switch (get_struct_type(t))
    {
    /*
     * RPC_FC_BYTE, RPC_FC_STRUCT, etc
     *  Simple types don't effect the type of struct.
     *  A struct containing a simple struct is still a simple struct.
     *  So long as we can block copy the data, we return RPC_FC_STRUCT.
     */
    case 0: /* void pointer */
    case RPC_FC_BYTE:
    case RPC_FC_CHAR:
    case RPC_FC_SMALL:
    case RPC_FC_USMALL:
    case RPC_FC_WCHAR:
    case RPC_FC_SHORT:
    case RPC_FC_USHORT:
    case RPC_FC_LONG:
    case RPC_FC_ULONG:
    case RPC_FC_INT3264:
    case RPC_FC_UINT3264:
    case RPC_FC_HYPER:
    case RPC_FC_FLOAT:
    case RPC_FC_DOUBLE:
    case RPC_FC_STRUCT:
    case RPC_FC_ENUM32:
      break;

    case RPC_FC_RP:
      return RPC_FC_BOGUS_STRUCT;

    case RPC_FC_UP:
    case RPC_FC_FP:
    case RPC_FC_OP:
      if (pointer_size != 4)
        return RPC_FC_BOGUS_STRUCT;
      /* pointers to interfaces aren't really pointers and have to be
       * marshalled specially so they make the structure complex */
      if (type_pointer_get_ref(t)->type == RPC_FC_IP)
        return RPC_FC_BOGUS_STRUCT;
      has_pointer = 1;
      break;

    case RPC_FC_SMFARRAY:
    case RPC_FC_LGFARRAY:
    case RPC_FC_SMVARRAY:
    case RPC_FC_LGVARRAY:
    case RPC_FC_CARRAY:
    case RPC_FC_CVARRAY:
    case RPC_FC_BOGUS_ARRAY:
    {
      unsigned int ptr_type = get_attrv(field->attrs, ATTR_POINTERTYPE);
      if (!ptr_type || ptr_type == RPC_FC_RP)
        return RPC_FC_BOGUS_STRUCT;
      else if (pointer_size != 4)
        return RPC_FC_BOGUS_STRUCT;
      has_pointer = 1;
      break;
    }

    /*
     * Propagate member attributes
     *  a struct should be at least as complex as its member
     */
    case RPC_FC_CVSTRUCT:
      has_conformance = 1;
      has_variance = 1;
      has_pointer = 1;
      break;

    case RPC_FC_CPSTRUCT:
      has_conformance = 1;
      if (list_next( fields, &field->entry ))
          error_loc("field '%s' deriving from a conformant array must be the last field in the structure\n",
                  field->name);
      has_pointer = 1;
      break;

    case RPC_FC_CSTRUCT:
      has_conformance = 1;
      if (list_next( fields, &field->entry ))
          error_loc("field '%s' deriving from a conformant array must be the last field in the structure\n",
                  field->name);
      break;

    case RPC_FC_PSTRUCT:
      has_pointer = 1;
      break;

    default:
      error_loc("Unknown struct member %s with type (0x%02x)\n", field->name, t->type);
      /* fallthru - treat it as complex */

    /* as soon as we see one of these these members, it's bogus... */
    case RPC_FC_ENCAPSULATED_UNION:
    case RPC_FC_NON_ENCAPSULATED_UNION:
    case RPC_FC_BOGUS_STRUCT:
    case RPC_FC_ENUM16:
      return RPC_FC_BOGUS_STRUCT;
    }
  }

  if( has_variance )
  {
    if ( has_conformance )
      return RPC_FC_CVSTRUCT;
    else
      return RPC_FC_BOGUS_STRUCT;
  }
  if( has_conformance && has_pointer )
    return RPC_FC_CPSTRUCT;
  if( has_conformance )
    return RPC_FC_CSTRUCT;
  if( has_pointer )
    return RPC_FC_PSTRUCT;
  return RPC_FC_STRUCT;
}

static unsigned char get_array_type(const type_t *type)
{
    unsigned char fc;
    const expr_t *size_is;
    const type_t *elem_type;

    if (!is_array(type))
        return type->type;

    elem_type = type_array_get_element(type);
    size_is = type_array_get_conformance(type);

    if (!size_is)
    {
        unsigned int align = 0;
        size_t size = type_memsize(elem_type, &align);
        if (size * type_array_get_dim(type) > 0xffffuL)
            fc = RPC_FC_LGFARRAY;
        else
            fc = RPC_FC_SMFARRAY;
    }
    else
        fc = RPC_FC_CARRAY;

    if (type_array_has_variance(type))
    {
        if (fc == RPC_FC_SMFARRAY)
            fc = RPC_FC_SMVARRAY;
        else if (fc == RPC_FC_LGFARRAY)
            fc = RPC_FC_LGVARRAY;
        else if (fc == RPC_FC_CARRAY)
            fc = RPC_FC_CVARRAY;
    }

    if (is_user_type(elem_type))
        fc = RPC_FC_BOGUS_ARRAY;
    else if (is_struct(elem_type->type))
    {
        switch (get_struct_type(elem_type))
        {
        case RPC_FC_BOGUS_STRUCT:
            fc = RPC_FC_BOGUS_ARRAY;
            break;
        }
    }
    else if (elem_type->type == RPC_FC_ENUM16)
    {
        /* is 16-bit enum - if so, wire size differs from mem size and so
         * the array cannot be block copied, which means the array is complex */
        fc = RPC_FC_BOGUS_ARRAY;
    }
    else if (is_union(elem_type->type))
        fc = RPC_FC_BOGUS_ARRAY;
    else if (is_ptr(elem_type))
    {
        /* ref pointers cannot just be block copied. unique pointers to
         * interfaces need special treatment. either case means the array is
         * complex */
        if (get_pointer_fc(elem_type) == RPC_FC_RP ||
            type_pointer_get_ref(elem_type)->type == RPC_FC_IP)
            fc = RPC_FC_BOGUS_ARRAY;
    }

    return fc;
}

int is_struct(unsigned char type)
{
    switch (type)
    {
    case RPC_FC_STRUCT:
    case RPC_FC_PSTRUCT:
    case RPC_FC_CSTRUCT:
    case RPC_FC_CPSTRUCT:
    case RPC_FC_CVSTRUCT:
    case RPC_FC_BOGUS_STRUCT:
        return 1;
    default:
        return 0;
    }
}

static int is_non_complex_struct(const type_t *type)
{
    switch (get_struct_type(type))
    {
    case RPC_FC_STRUCT:
    case RPC_FC_PSTRUCT:
    case RPC_FC_CSTRUCT:
    case RPC_FC_CPSTRUCT:
    case RPC_FC_CVSTRUCT:
        return 1;
    default:
        return 0;
    }
}

int is_union(unsigned char type)
{
    switch (type)
    {
    case RPC_FC_ENCAPSULATED_UNION:
    case RPC_FC_NON_ENCAPSULATED_UNION:
        return 1;
    default:
        return 0;
    }
}

static int type_has_pointers(const type_t *type)
{
    if (is_user_type(type))
        return FALSE;
    else if (is_ptr(type))
        return TRUE;
    else if (is_array(type))
        return type_has_pointers(type_array_get_element(type));
    else if (is_struct(type->type))
    {
        var_list_t *fields = type_struct_get_fields(type);
        const var_t *field;
        if (fields) LIST_FOR_EACH_ENTRY( field, fields, const var_t, entry )
        {
            if (type_has_pointers(field->type))
                return TRUE;
        }
    }
    else if (is_union(type->type))
    {
        var_list_t *fields;
        const var_t *field;
        fields = type_union_get_cases(type);
        if (fields) LIST_FOR_EACH_ENTRY( field, fields, const var_t, entry )
        {
            if (field->type && type_has_pointers(field->type))
                return TRUE;
        }
    }

    return FALSE;
}

static int type_has_full_pointer(const type_t *type)
{
    if (is_user_type(type))
        return FALSE;
    else if (type->type == RPC_FC_FP)
        return TRUE;
    else if (is_ptr(type))
        return FALSE;
    else if (is_array(type))
        return type_has_full_pointer(type_array_get_element(type));
    else if (is_struct(type->type))
    {
        var_list_t *fields = type_struct_get_fields(type);
        const var_t *field;
        if (fields) LIST_FOR_EACH_ENTRY( field, fields, const var_t, entry )
        {
            if (type_has_full_pointer(field->type))
                return TRUE;
        }
    }
    else if (is_union(type->type))
    {
        var_list_t *fields;
        const var_t *field;
        fields = type_union_get_cases(type);
        if (fields) LIST_FOR_EACH_ENTRY( field, fields, const var_t, entry )
        {
            if (field->type && type_has_full_pointer(field->type))
                return TRUE;
        }
    }

    return FALSE;
}

static unsigned short user_type_offset(const char *name)
{
    user_type_t *ut;
    unsigned short off = 0;
    LIST_FOR_EACH_ENTRY(ut, &user_type_list, user_type_t, entry)
    {
        if (strcmp(name, ut->name) == 0)
            return off;
        ++off;
    }
    error("user_type_offset: couldn't find type (%s)\n", name);
    return 0;
}

static void update_tfsoff(type_t *type, unsigned int offset, FILE *file)
{
    type->typestring_offset = offset;
    if (file) type->tfswrite = FALSE;
}

static void guard_rec(type_t *type)
{
    /* types that contain references to themselves (like a linked list),
       need to be shielded from infinite recursion when writing embedded
       types  */
    if (type->typestring_offset)
        type->tfswrite = FALSE;
    else
        type->typestring_offset = 1;
}

static type_t *get_user_type(const type_t *t, const char **pname)
{
    for (;;)
    {
        type_t *ut = get_attrp(t->attrs, ATTR_WIREMARSHAL);
        if (ut)
        {
            if (pname)
                *pname = t->name;
            return ut;
        }

        if (type_is_alias(t))
            t = t->orig;
        else
            return 0;
    }
}

int is_user_type(const type_t *t)
{
    return get_user_type(t, NULL) != NULL;
}

static int is_embedded_complex(const type_t *type)
{
    unsigned char tc = type->type;
    return is_struct(tc) || is_union(tc) || is_array(type) || is_user_type(type)
        || (is_ptr(type) && type_pointer_get_ref(type)->type == RPC_FC_IP);
}

static const char *get_context_handle_type_name(const type_t *type)
{
    const type_t *t;
    for (t = type; is_ptr(t); t = type_pointer_get_ref(t))
        if (is_attr(t->attrs, ATTR_CONTEXTHANDLE))
            return t->name;
    assert(0);
    return NULL;
}

#define WRITE_FCTYPE(file, fctype, typestring_offset) \
    do { \
        if (file) \
            fprintf(file, "/* %2u */\n", typestring_offset); \
        print_file((file), 2, "0x%02x,    /* " #fctype " */\n", RPC_##fctype); \
    } \
    while (0)

static void print_file(FILE *file, int indent, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    print(file, indent, format, va);
    va_end(va);
}

void print(FILE *file, int indent, const char *format, va_list va)
{
    if (file)
    {
        if (format[0] != '\n')
            while (0 < indent--)
                fprintf(file, "    ");
        vfprintf(file, format, va);
    }
}


static void write_var_init(FILE *file, int indent, const type_t *t, const char *n, const char *local_var_prefix)
{
    if (decl_indirect(t))
    {
        print_file(file, indent, "MIDL_memset(&%s%s, 0, sizeof(%s%s));\n",
                   local_var_prefix, n, local_var_prefix, n);
        print_file(file, indent, "%s_p_%s = &%s%s;\n", local_var_prefix, n, local_var_prefix, n);
    }
    else if (is_ptr(t) || is_array(t))
        print_file(file, indent, "%s%s = 0;\n", local_var_prefix, n);
}

void write_parameters_init(FILE *file, int indent, const var_t *func, const char *local_var_prefix)
{
    const var_t *var;

    if (!is_void(type_function_get_rettype(func->type)))
        write_var_init(file, indent, type_function_get_rettype(func->type), "_RetVal", local_var_prefix);

    if (!type_get_function_args(func->type))
        return;

    LIST_FOR_EACH_ENTRY( var, type_get_function_args(func->type), const var_t, entry )
        write_var_init(file, indent, var->type, var->name, local_var_prefix);

    fprintf(file, "\n");
}

static void write_formatdesc(FILE *f, int indent, const char *str)
{
    print_file(f, indent, "typedef struct _MIDL_%s_FORMAT_STRING\n", str);
    print_file(f, indent, "{\n");
    print_file(f, indent + 1, "short Pad;\n");
    print_file(f, indent + 1, "unsigned char Format[%s_FORMAT_STRING_SIZE];\n", str);
    print_file(f, indent, "} MIDL_%s_FORMAT_STRING;\n", str);
    print_file(f, indent, "\n");
}

void write_formatstringsdecl(FILE *f, int indent, const statement_list_t *stmts, type_pred_t pred)
{
    clear_all_offsets();

    print_file(f, indent, "#define TYPE_FORMAT_STRING_SIZE %d\n",
               get_size_typeformatstring(stmts, pred));

    print_file(f, indent, "#define PROC_FORMAT_STRING_SIZE %d\n",
               get_size_procformatstring(stmts, pred));

    fprintf(f, "\n");
    write_formatdesc(f, indent, "TYPE");
    write_formatdesc(f, indent, "PROC");
    fprintf(f, "\n");
    print_file(f, indent, "static const MIDL_TYPE_FORMAT_STRING __MIDL_TypeFormatString;\n");
    print_file(f, indent, "static const MIDL_PROC_FORMAT_STRING __MIDL_ProcFormatString;\n");
    print_file(f, indent, "\n");
}

static inline int is_base_type(unsigned char type)
{
    switch (type)
    {
    case RPC_FC_BYTE:
    case RPC_FC_CHAR:
    case RPC_FC_USMALL:
    case RPC_FC_SMALL:
    case RPC_FC_WCHAR:
    case RPC_FC_USHORT:
    case RPC_FC_SHORT:
    case RPC_FC_ULONG:
    case RPC_FC_LONG:
    case RPC_FC_HYPER:
    case RPC_FC_IGNORE:
    case RPC_FC_FLOAT:
    case RPC_FC_DOUBLE:
    case RPC_FC_ENUM16:
    case RPC_FC_ENUM32:
    case RPC_FC_ERROR_STATUS_T:
    case RPC_FC_BIND_PRIMITIVE:
        return TRUE;

    default:
        return FALSE;
    }
}

int decl_indirect(const type_t *t)
{
    return is_user_type(t)
        || (!is_base_type(t->type)
            && !is_ptr(t)
            && !is_array(t));
}

static size_t write_procformatstring_type(FILE *file, int indent,
                                          const char *name,
                                          const type_t *type,
                                          const attr_list_t *attrs,
                                          int is_return)
{
    size_t size;

    int is_in = is_attr(attrs, ATTR_IN);
    int is_out = is_attr(attrs, ATTR_OUT);

    if (!is_in && !is_out) is_in = TRUE;

    if (!type->declarray && is_base_type(type->type))
    {
        if (is_return)
            print_file(file, indent, "0x53,    /* FC_RETURN_PARAM_BASETYPE */\n");
        else
            print_file(file, indent, "0x4e,    /* FC_IN_PARAM_BASETYPE */\n");

        if (type->type == RPC_FC_BIND_PRIMITIVE)
        {
            print_file(file, indent, "0x%02x,    /* FC_IGNORE */\n", RPC_FC_IGNORE);
            size = 2; /* includes param type prefix */
        }
        else if (is_base_type(type->type))
        {
            print_file(file, indent, "0x%02x,    /* %s */\n", type->type, string_of_type(type->type));
            size = 2; /* includes param type prefix */
        }
        else
        {
            error("Unknown/unsupported type: %s (0x%02x)\n", name, type->type);
            size = 0;
        }
    }
    else
    {
        if (is_return)
            print_file(file, indent, "0x52,    /* FC_RETURN_PARAM */\n");
        else if (is_in && is_out)
            print_file(file, indent, "0x50,    /* FC_IN_OUT_PARAM */\n");
        else if (is_out)
            print_file(file, indent, "0x51,    /* FC_OUT_PARAM */\n");
        else
            print_file(file, indent, "0x4d,    /* FC_IN_PARAM */\n");

        print_file(file, indent, "0x01,\n");
        print_file(file, indent, "NdrFcShort(0x%x),\n", type->typestring_offset);
        size = 4; /* includes param type prefix */
    }
    return size;
}

static void write_procformatstring_stmts(FILE *file, int indent, const statement_list_t *stmts, type_pred_t pred)
{
    const statement_t *stmt;
    if (stmts) LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
    {
        if (stmt->type == STMT_TYPE && stmt->u.type->type == RPC_FC_IP)
        {
            const statement_t *stmt_func;
            if (!pred(stmt->u.type))
                continue;
            STATEMENTS_FOR_EACH_FUNC(stmt_func, type_iface_get_stmts(stmt->u.type))
            {
                const var_t *func = stmt_func->u.var;
                if (is_local(func->attrs)) continue;
                /* emit argument data */
                if (type_get_function_args(func->type))
                {
                    const var_t *var;
                    LIST_FOR_EACH_ENTRY( var, type_get_function_args(func->type), const var_t, entry )
                        write_procformatstring_type(file, indent, var->name, var->type, var->attrs, FALSE);
                }

                /* emit return value data */
                if (is_void(type_function_get_rettype(func->type)))
                {
                    print_file(file, indent, "0x5b,    /* FC_END */\n");
                    print_file(file, indent, "0x5c,    /* FC_PAD */\n");
                }
                else
                    write_procformatstring_type(file, indent, "return value", type_function_get_rettype(func->type), NULL, TRUE);
            }
        }
        else if (stmt->type == STMT_LIBRARY)
            write_procformatstring_stmts(file, indent, stmt->u.lib->stmts, pred);
    }
}

void write_procformatstring(FILE *file, const statement_list_t *stmts, type_pred_t pred)
{
    int indent = 0;

    print_file(file, indent, "static const MIDL_PROC_FORMAT_STRING __MIDL_ProcFormatString =\n");
    print_file(file, indent, "{\n");
    indent++;
    print_file(file, indent, "0,\n");
    print_file(file, indent, "{\n");
    indent++;

    write_procformatstring_stmts(file, indent, stmts, pred);

    print_file(file, indent, "0x0\n");
    indent--;
    print_file(file, indent, "}\n");
    indent--;
    print_file(file, indent, "};\n");
    print_file(file, indent, "\n");
}

static int write_base_type(FILE *file, unsigned char fc, unsigned int *typestring_offset)
{
    if (is_base_type(fc))
    {
        print_file(file, 2, "0x%02x,\t/* %s */\n", fc, string_of_type(fc));
        *typestring_offset += 1;
        return 1;
    }

    return 0;
}

/* write conformance / variance descriptor */
static size_t write_conf_or_var_desc(FILE *file, const type_t *structure,
                                     unsigned int baseoff, const type_t *type,
                                     const expr_t *expr)
{
    unsigned char operator_type = 0;
    unsigned char conftype = RPC_FC_NORMAL_CONFORMANCE;
    const char *conftype_string = "";
    const char *operator_string = "no operators";
    const expr_t *subexpr;

    if (!expr)
    {
        print_file(file, 2, "NdrFcLong(0xffffffff),\t/* -1 */\n");
        return 4;
    }

    if (!structure)
    {
        /* Top-level conformance calculations are done inline.  */
        print_file (file, 2, "0x%x,\t/* Corr desc: parameter */\n",
                    RPC_FC_TOP_LEVEL_CONFORMANCE);
        print_file (file, 2, "0x0,\n");
        print_file (file, 2, "NdrFcShort(0x0),\n");
        return 4;
    }

    if (expr->is_const)
    {
        if (expr->cval > UCHAR_MAX * (USHRT_MAX + 1) + USHRT_MAX)
            error("write_conf_or_var_desc: constant value %ld is greater than "
                  "the maximum constant size of %d\n", expr->cval,
                  UCHAR_MAX * (USHRT_MAX + 1) + USHRT_MAX);

        print_file(file, 2, "0x%x, /* Corr desc: constant, val = %ld */\n",
                   RPC_FC_CONSTANT_CONFORMANCE, expr->cval);
        print_file(file, 2, "0x%x,\n", expr->cval & ~USHRT_MAX);
        print_file(file, 2, "NdrFcShort(0x%x),\n", expr->cval & USHRT_MAX);

        return 4;
    }

    if (is_ptr(type) || (is_array(type) && !type->declarray))
    {
        conftype = RPC_FC_POINTER_CONFORMANCE;
        conftype_string = "field pointer, ";
    }

    subexpr = expr;
    switch (subexpr->type)
    {
    case EXPR_PPTR:
        subexpr = subexpr->ref;
        operator_type = RPC_FC_DEREFERENCE;
        operator_string = "FC_DEREFERENCE";
        break;
    case EXPR_DIV:
        if (subexpr->u.ext->is_const && (subexpr->u.ext->cval == 2))
        {
            subexpr = subexpr->ref;
            operator_type = RPC_FC_DIV_2;
            operator_string = "FC_DIV_2";
        }
        break;
    case EXPR_MUL:
        if (subexpr->u.ext->is_const && (subexpr->u.ext->cval == 2))
        {
            subexpr = subexpr->ref;
            operator_type = RPC_FC_MULT_2;
            operator_string = "FC_MULT_2";
        }
        break;
    case EXPR_SUB:
        if (subexpr->u.ext->is_const && (subexpr->u.ext->cval == 1))
        {
            subexpr = subexpr->ref;
            operator_type = RPC_FC_SUB_1;
            operator_string = "FC_SUB_1";
        }
        break;
    case EXPR_ADD:
        if (subexpr->u.ext->is_const && (subexpr->u.ext->cval == 1))
        {
            subexpr = subexpr->ref;
            operator_type = RPC_FC_ADD_1;
            operator_string = "FC_ADD_1";
        }
        break;
    default:
        break;
    }

    if (subexpr->type == EXPR_IDENTIFIER)
    {
        const type_t *correlation_variable = NULL;
        unsigned char correlation_variable_type;
        unsigned char param_type = 0;
        size_t offset = 0;
        const var_t *var;
        var_list_t *fields = type_struct_get_fields(structure);

        if (fields) LIST_FOR_EACH_ENTRY( var, fields, const var_t, entry )
        {
            unsigned int align = 0;
            /* FIXME: take alignment into account */
            if (var->name && !strcmp(var->name, subexpr->u.sval))
            {
                correlation_variable = var->type;
                break;
            }
            offset += type_memsize(var->type, &align);
        }
        if (!correlation_variable)
            error("write_conf_or_var_desc: couldn't find variable %s in structure\n",
                  subexpr->u.sval);

        correlation_variable = expr_resolve_type(NULL, structure, expr);

        offset -= baseoff;
        correlation_variable_type = correlation_variable->type;

        switch (correlation_variable_type)
        {
        case RPC_FC_CHAR:
        case RPC_FC_SMALL:
            param_type = RPC_FC_SMALL;
            break;
        case RPC_FC_BYTE:
        case RPC_FC_USMALL:
            param_type = RPC_FC_USMALL;
            break;
        case RPC_FC_WCHAR:
        case RPC_FC_SHORT:
        case RPC_FC_ENUM16:
            param_type = RPC_FC_SHORT;
            break;
        case RPC_FC_USHORT:
            param_type = RPC_FC_USHORT;
            break;
        case RPC_FC_LONG:
        case RPC_FC_ENUM32:
            param_type = RPC_FC_LONG;
            break;
        case RPC_FC_ULONG:
            param_type = RPC_FC_ULONG;
            break;
        default:
            error("write_conf_or_var_desc: conformance variable type not supported 0x%x\n",
                correlation_variable_type);
        }

        print_file(file, 2, "0x%x, /* Corr desc: %s%s */\n",
                   conftype | param_type, conftype_string, string_of_type(param_type));
        print_file(file, 2, "0x%x, /* %s */\n", operator_type, operator_string);
        print_file(file, 2, "NdrFcShort(0x%x), /* offset = %d */\n",
                   offset, offset);
    }
    else
    {
        unsigned int callback_offset = 0;
        struct expr_eval_routine *eval;
        int found = 0;

        LIST_FOR_EACH_ENTRY(eval, &expr_eval_routines, struct expr_eval_routine, entry)
        {
            if (!strcmp (eval->structure->name, structure->name)
                && !compare_expr (eval->expr, expr))
            {
                found = 1;
                break;
            }
            callback_offset++;
        }

        if (!found)
        {
            eval = xmalloc (sizeof(*eval));
            eval->structure = structure;
            eval->baseoff = baseoff;
            eval->expr = expr;
            list_add_tail (&expr_eval_routines, &eval->entry);
        }

        if (callback_offset > USHRT_MAX)
            error("Maximum number of callback routines reached\n");

        print_file(file, 2, "0x%x, /* Corr desc: %s */\n", conftype, conftype_string);
        print_file(file, 2, "0x%x, /* %s */\n", RPC_FC_CALLBACK, "FC_CALLBACK");
        print_file(file, 2, "NdrFcShort(0x%x), /* %u */\n", callback_offset, callback_offset);
    }
    return 4;
}

static size_t fields_memsize(const var_list_t *fields, unsigned int *align)
{
    int have_align = FALSE;
    size_t size = 0;
    const var_t *v;

    if (!fields) return 0;
    LIST_FOR_EACH_ENTRY( v, fields, const var_t, entry )
    {
        unsigned int falign = 0;
        size_t fsize = type_memsize(v->type, &falign);
        if (!have_align)
        {
            *align = falign;
            have_align = TRUE;
        }
        size = ROUND_SIZE(size, falign);
        size += fsize;
    }

    size = ROUND_SIZE(size, *align);
    return size;
}

static size_t union_memsize(const var_list_t *fields, unsigned int *pmaxa)
{
    size_t size, maxs = 0;
    unsigned int align = *pmaxa;
    const var_t *v;

    if (fields) LIST_FOR_EACH_ENTRY( v, fields, const var_t, entry )
    {
        /* we could have an empty default field with NULL type */
        if (v->type)
        {
            size = type_memsize(v->type, &align);
            if (maxs < size) maxs = size;
            if (*pmaxa < align) *pmaxa = align;
        }
    }

    return maxs;
}

int get_padding(const var_list_t *fields)
{
    unsigned short offset = 0;
    int salign = -1;
    const var_t *f;

    if (!fields)
        return 0;

    LIST_FOR_EACH_ENTRY(f, fields, const var_t, entry)
    {
        type_t *ft = f->type;
        unsigned int align = 0;
        size_t size = type_memsize(ft, &align);
        if (salign == -1)
            salign = align;
        offset = ROUND_SIZE(offset, align);
        offset += size;
    }

    return ROUNDING(offset, salign);
}

size_t type_memsize(const type_t *t, unsigned int *align)
{
    size_t size = 0;

    if (type_is_alias(t))
        size = type_memsize(t->orig, align);
    else if (t->declarray && is_conformant_array(t))
    {
        type_memsize(type_array_get_element(t), align);
        size = 0;
    }
    else if (is_ptr(t) || is_conformant_array(t))
    {
        assert( pointer_size );
        size = pointer_size;
        if (size > *align) *align = size;
    }
    else switch (t->type)
    {
    case RPC_FC_BYTE:
    case RPC_FC_CHAR:
    case RPC_FC_USMALL:
    case RPC_FC_SMALL:
        size = 1;
        if (size > *align) *align = size;
        break;
    case RPC_FC_WCHAR:
    case RPC_FC_USHORT:
    case RPC_FC_SHORT:
    case RPC_FC_ENUM16:
        size = 2;
        if (size > *align) *align = size;
        break;
    case RPC_FC_ULONG:
    case RPC_FC_LONG:
    case RPC_FC_ERROR_STATUS_T:
    case RPC_FC_ENUM32:
    case RPC_FC_FLOAT:
        size = 4;
        if (size > *align) *align = size;
        break;
    case RPC_FC_HYPER:
    case RPC_FC_DOUBLE:
        size = 8;
        if (size > *align) *align = size;
        break;
    case RPC_FC_STRUCT:
    case RPC_FC_CVSTRUCT:
    case RPC_FC_CPSTRUCT:
    case RPC_FC_CSTRUCT:
    case RPC_FC_PSTRUCT:
    case RPC_FC_BOGUS_STRUCT:
        size = fields_memsize(type_struct_get_fields(t), align);
        break;
    case RPC_FC_ENCAPSULATED_UNION:
        size = fields_memsize(type_encapsulated_union_get_fields(t), align);
        break;
    case RPC_FC_NON_ENCAPSULATED_UNION:
        size = union_memsize(type_union_get_cases(t), align);
        break;
    case RPC_FC_SMFARRAY:
    case RPC_FC_LGFARRAY:
    case RPC_FC_SMVARRAY:
    case RPC_FC_LGVARRAY:
    case RPC_FC_BOGUS_ARRAY:
        size = type_array_get_dim(t) * type_memsize(type_array_get_element(t), align);
        break;
    default:
        error("type_memsize: Unknown type 0x%x\n", t->type);
        size = 0;
    }

    return size;
}

int is_full_pointer_function(const var_t *func)
{
    const var_t *var;
    if (type_has_full_pointer(type_function_get_rettype(func->type)))
        return TRUE;
    if (!type_get_function_args(func->type))
        return FALSE;
    LIST_FOR_EACH_ENTRY( var, type_get_function_args(func->type), const var_t, entry )
        if (type_has_full_pointer( var->type ))
            return TRUE;
    return FALSE;
}

void write_full_pointer_init(FILE *file, int indent, const var_t *func, int is_server)
{
    print_file(file, indent, "__frame->_StubMsg.FullPtrXlatTables = NdrFullPointerXlatInit(0,%s);\n",
                   is_server ? "XLAT_SERVER" : "XLAT_CLIENT");
    fprintf(file, "\n");
}

void write_full_pointer_free(FILE *file, int indent, const var_t *func)
{
    print_file(file, indent, "NdrFullPointerXlatFree(__frame->_StubMsg.FullPtrXlatTables);\n");
    fprintf(file, "\n");
}

static unsigned int write_nonsimple_pointer(FILE *file, const type_t *type, size_t offset)
{
    short absoff = type_pointer_get_ref(type)->typestring_offset;
    short reloff = absoff - (offset + 2);
    int ptr_attr = is_ptr(type_pointer_get_ref(type)) ? 0x10 : 0x0;

    print_file(file, 2, "0x%02x, 0x%x,\t/* %s */\n",
               type->type, ptr_attr, string_of_type(type->type));
    print_file(file, 2, "NdrFcShort(0x%hx),\t/* Offset= %hd (%hd) */\n",
               reloff, reloff, absoff);
    return 4;
}

static unsigned int write_simple_pointer(FILE *file, const type_t *type)
{
    unsigned char fc = type_pointer_get_ref(type)->type;
    /* for historical reasons, write_simple_pointer also handled string types,
     * but no longer does. catch bad uses of the function with this check */
    if (is_string_type(type->attrs, type))
        error("write_simple_pointer: can't handle type %s which is a string type\n", type->name);
    print_file(file, 2, "0x%02x, 0x8,\t/* %s [simple_pointer] */\n",
               type->type, string_of_type(type->type));
    print_file(file, 2, "0x%02x,\t/* %s */\n", fc, string_of_type(fc));
    print_file(file, 2, "0x5c,\t/* FC_PAD */\n");
    return 4;
}

static void print_start_tfs_comment(FILE *file, type_t *t, unsigned int tfsoff)
{
    print_file(file, 0, "/* %u (", tfsoff);
    write_type_decl(file, t, NULL);
    print_file(file, 0, ") */\n");
}

static size_t write_pointer_tfs(FILE *file, type_t *type, unsigned int *typestring_offset)
{
    unsigned int offset = *typestring_offset;

    print_start_tfs_comment(file, type, offset);
    update_tfsoff(type, offset, file);

    if (type_pointer_get_ref(type)->typestring_offset)
        *typestring_offset += write_nonsimple_pointer(file, type, offset);
    else if (is_base_type(type_pointer_get_ref(type)->type))
        *typestring_offset += write_simple_pointer(file, type);

    return offset;
}

static int processed(const type_t *type)
{
    return type->typestring_offset && !type->tfswrite;
}

static int user_type_has_variable_size(const type_t *t)
{
    if (is_ptr(t))
        return TRUE;
    else
        switch (get_struct_type(t))
        {
        case RPC_FC_PSTRUCT:
        case RPC_FC_CSTRUCT:
        case RPC_FC_CPSTRUCT:
        case RPC_FC_CVSTRUCT:
            return TRUE;
        }
    /* Note: Since this only applies to user types, we can't have a conformant
       array here, and strings should get filed under pointer in this case.  */
    return FALSE;
}

static void write_user_tfs(FILE *file, type_t *type, unsigned int *tfsoff)
{
    unsigned int start, absoff, flags;
    unsigned int align = 0, ualign = 0;
    const char *name = NULL;
    type_t *utype = get_user_type(type, &name);
    size_t usize = user_type_has_variable_size(utype) ? 0 : type_memsize(utype, &ualign);
    size_t size = type_memsize(type, &align);
    unsigned short funoff = user_type_offset(name);
    short reloff;

    guard_rec(type);

    if (is_base_type(utype->type))
    {
        absoff = *tfsoff;
        print_start_tfs_comment(file, utype, absoff);
        print_file(file, 2, "0x%x,\t/* %s */\n", utype->type, string_of_type(utype->type));
        print_file(file, 2, "0x5c,\t/* FC_PAD */\n");
        *tfsoff += 2;
    }
    else
    {
        if (!processed(utype))
            write_embedded_types(file, NULL, utype, utype->name, TRUE, tfsoff);
        absoff = utype->typestring_offset;
    }

    if (utype->type == RPC_FC_RP)
        flags = 0x40;
    else if (utype->type == RPC_FC_UP)
        flags = 0x80;
    else
        flags = 0;

    start = *tfsoff;
    update_tfsoff(type, start, file);
    print_start_tfs_comment(file, type, start);
    print_file(file, 2, "0x%x,\t/* FC_USER_MARSHAL */\n", RPC_FC_USER_MARSHAL);
    print_file(file, 2, "0x%x,\t/* Alignment= %d, Flags= %02x */\n",
               flags | (align - 1), align - 1, flags);
    print_file(file, 2, "NdrFcShort(0x%hx),\t/* Function offset= %hu */\n", funoff, funoff);
    print_file(file, 2, "NdrFcShort(0x%lx),\t/* %lu */\n", size, size);
    print_file(file, 2, "NdrFcShort(0x%lx),\t/* %lu */\n", usize, usize);
    *tfsoff += 8;
    reloff = absoff - *tfsoff;
    print_file(file, 2, "NdrFcShort(0x%hx),\t/* Offset= %hd (%lu) */\n", reloff, reloff, absoff);
    *tfsoff += 2;
}

static inline unsigned char make_signed(unsigned char fc)
{
    switch(fc)
    {
        case RPC_FC_USMALL:
            return RPC_FC_SMALL;
        case RPC_FC_USHORT:
            return RPC_FC_SHORT;
        case RPC_FC_ULONG:
            return RPC_FC_LONG;
        default:
            return fc;
    }
}

static void write_member_type(FILE *file, const type_t *cont,
                              const attr_list_t *attrs, const type_t *type,
                              unsigned int *corroff, unsigned int *tfsoff)
{
    if (is_embedded_complex(type) && !is_conformant_array(type))
    {
        size_t absoff;
        short reloff;

        if (is_union(type->type) && is_attr(attrs, ATTR_SWITCHIS))
        {
            absoff = *corroff;
            *corroff += 8;
        }
        else
        {
            absoff = type->typestring_offset;
        }
        reloff = absoff - (*tfsoff + 2);

        print_file(file, 2, "0x4c,\t/* FC_EMBEDDED_COMPLEX */\n");
        /* FIXME: actually compute necessary padding */
        print_file(file, 2, "0x0,\t/* FIXME: padding */\n");
        print_file(file, 2, "NdrFcShort(0x%hx),\t/* Offset= %hd (%lu) */\n",
                   reloff, reloff, absoff);
        *tfsoff += 4;
    }
    else if (is_ptr(type) || is_conformant_array(type))
    {
        unsigned char fc = (get_struct_type(cont) == RPC_FC_BOGUS_STRUCT
                            ? RPC_FC_POINTER
                            : RPC_FC_LONG);
        print_file(file, 2, "0x%x,\t/* %s */\n", fc, string_of_type(fc));
        *tfsoff += 1;
    }
    else if (!write_base_type(file, make_signed(type->type), tfsoff))
        error("Unsupported member type 0x%x\n", type->type);
}

static void write_end(FILE *file, unsigned int *tfsoff)
{
    if (*tfsoff % 2 == 0)
    {
        print_file(file, 2, "0x%x,\t\t/* FC_PAD */\n", RPC_FC_PAD);
        *tfsoff += 1;
    }
    print_file(file, 2, "0x%x,\t\t/* FC_END */\n", RPC_FC_END);
    *tfsoff += 1;
}

static void write_descriptors(FILE *file, type_t *type, unsigned int *tfsoff)
{
    unsigned int offset = 0;
    var_list_t *fs = type_struct_get_fields(type);
    var_t *f;

    if (fs) LIST_FOR_EACH_ENTRY(f, fs, var_t, entry)
    {
        unsigned int align = 0;
        type_t *ft = f->type;
        if (is_union(ft->type) && is_attr(f->attrs, ATTR_SWITCHIS))
        {
            unsigned int absoff = ft->typestring_offset;
            short reloff = absoff - (*tfsoff + 6);
            print_file(file, 0, "/* %d */\n", *tfsoff);
            print_file(file, 2, "0x%x,\t/* %s */\n", ft->type, string_of_type(ft->type));
            print_file(file, 2, "0x%x,\t/* FIXME: always FC_LONG */\n", RPC_FC_LONG);
            write_conf_or_var_desc(file, current_structure, offset, ft,
                                   get_attrp(f->attrs, ATTR_SWITCHIS));
            print_file(file, 2, "NdrFcShort(%hd),\t/* Offset= %hd (%u) */\n",
                       reloff, reloff, absoff);
            *tfsoff += 8;
        }

        /* FIXME: take alignment into account */
        offset += type_memsize(ft, &align);
    }
}

static int write_no_repeat_pointer_descriptions(
    FILE *file, type_t *type,
    size_t *offset_in_memory, size_t *offset_in_buffer,
    unsigned int *typestring_offset)
{
    int written = 0;
    unsigned int align;

    if (is_ptr(type) || (!type->declarray && is_conformant_array(type)))
    {
        size_t memsize;

        print_file(file, 2, "0x%02x, /* FC_NO_REPEAT */\n", RPC_FC_NO_REPEAT);
        print_file(file, 2, "0x%02x, /* FC_PAD */\n", RPC_FC_PAD);

        /* pointer instance */
        print_file(file, 2, "NdrFcShort(0x%x), /* Memory offset = %d */\n", *offset_in_memory, *offset_in_memory);
        print_file(file, 2, "NdrFcShort(0x%x), /* Buffer offset = %d */\n", *offset_in_buffer, *offset_in_buffer);
        *typestring_offset += 6;

        if (is_ptr(type))
        {
            if (is_string_type(type->attrs, type))
                write_string_tfs(file, NULL, type, NULL, typestring_offset);
            else
                write_pointer_tfs(file, type, typestring_offset);
        }
        else
        {
            unsigned absoff = type->typestring_offset;
            short reloff = absoff - (*typestring_offset + 2);
            /* FIXME: get pointer attributes from field */
            print_file(file, 2, "0x%02x, 0x0,\t/* %s */\n", RPC_FC_UP, "FC_UP");
            print_file(file, 2, "NdrFcShort(0x%hx),\t/* Offset= %hd (%u) */\n",
                       reloff, reloff, absoff);
            *typestring_offset += 4;
        }

        align = 0;
        memsize = type_memsize(type, &align);
        *offset_in_memory += memsize;
        /* increment these separately as in the case of conformant (varying)
         * structures these start at different values */
        *offset_in_buffer += memsize;

        return 1;
    }

    if (is_non_complex_struct(type))
    {
        const var_t *v;
        LIST_FOR_EACH_ENTRY( v, type_struct_get_fields(type), const var_t, entry )
        {
            if (offset_in_memory && offset_in_buffer)
            {
                size_t padding;
                align = 0;
                type_memsize(v->type, &align);
                padding = ROUNDING(*offset_in_memory, align);
                *offset_in_memory += padding;
                *offset_in_buffer += padding;
            }
            written += write_no_repeat_pointer_descriptions(
                file, v->type,
                offset_in_memory, offset_in_buffer, typestring_offset);
        }
    }
    else
    {
        size_t memsize;
        align = 0;
        memsize = type_memsize(type, &align);
        *offset_in_memory += memsize;
        /* increment these separately as in the case of conformant (varying)
         * structures these start at different values */
        *offset_in_buffer += memsize;
    }

    return written;
}

static int write_pointer_description_offsets(
    FILE *file, const attr_list_t *attrs, type_t *type,
    size_t *offset_in_memory, size_t *offset_in_buffer,
    unsigned int *typestring_offset)
{
    int written = 0;
    unsigned int align;

    if (is_ptr(type) && type_pointer_get_ref(type)->type != RPC_FC_IP)
    {
        if (offset_in_memory && offset_in_buffer)
        {
            size_t memsize;

            /* pointer instance */
            /* FIXME: sometimes from end of structure, sometimes from beginning */
            print_file(file, 2, "NdrFcShort(0x%x), /* Memory offset = %d */\n", *offset_in_memory, *offset_in_memory);
            print_file(file, 2, "NdrFcShort(0x%x), /* Buffer offset = %d */\n", *offset_in_buffer, *offset_in_buffer);

            align = 0;
            memsize = type_memsize(type, &align);
            *offset_in_memory += memsize;
            /* increment these separately as in the case of conformant (varying)
             * structures these start at different values */
            *offset_in_buffer += memsize;
        }
        *typestring_offset += 4;

        if (is_string_type(attrs, type))
            write_string_tfs(file, NULL, type, NULL, typestring_offset);
        else if (processed(type_pointer_get_ref(type)) ||
                 is_base_type(type_pointer_get_ref(type)->type))
            write_pointer_tfs(file, type, typestring_offset);
        else
            error("write_pointer_description_offsets: type format string unknown\n");

        return 1;
    }

    if (is_array(type))
    {
        return write_pointer_description_offsets(
            file, attrs, type_array_get_element(type), offset_in_memory,
            offset_in_buffer, typestring_offset);
    }
    else if (is_non_complex_struct(type))
    {
        /* otherwise search for interesting fields to parse */
        const var_t *v;
        LIST_FOR_EACH_ENTRY( v, type_struct_get_fields(type), const var_t, entry )
        {
            if (offset_in_memory && offset_in_buffer)
            {
                size_t padding;
                align = 0;
                type_memsize(v->type, &align);
                padding = ROUNDING(*offset_in_memory, align);
                *offset_in_memory += padding;
                *offset_in_buffer += padding;
            }
            written += write_pointer_description_offsets(
                file, v->attrs, v->type, offset_in_memory, offset_in_buffer,
                typestring_offset);
        }
    }
    else
    {
        if (offset_in_memory && offset_in_buffer)
        {
            size_t memsize;
            align = 0;
            memsize = type_memsize(type, &align);
            *offset_in_memory += memsize;
            /* increment these separately as in the case of conformant (varying)
             * structures these start at different values */
            *offset_in_buffer += memsize;
        }
    }

    return written;
}

/* Note: if file is NULL return value is number of pointers to write, else
 * it is the number of type format characters written */
static int write_fixed_array_pointer_descriptions(
    FILE *file, const attr_list_t *attrs, type_t *type,
    size_t *offset_in_memory, size_t *offset_in_buffer,
    unsigned int *typestring_offset)
{
    unsigned int align;
    int pointer_count = 0;
    int real_type = get_array_type( type );

    if (real_type == RPC_FC_SMFARRAY || real_type == RPC_FC_LGFARRAY)
    {
        unsigned int temp = 0;
        /* unfortunately, this needs to be done in two passes to avoid
         * writing out redundant FC_FIXED_REPEAT descriptions */
        pointer_count = write_pointer_description_offsets(
            NULL, attrs, type_array_get_element(type), NULL, NULL, &temp);
        if (pointer_count > 0)
        {
            unsigned int increment_size;
            size_t offset_of_array_pointer_mem = 0;
            size_t offset_of_array_pointer_buf = 0;

            align = 0;
            increment_size = type_memsize(type_array_get_element(type), &align);

            print_file(file, 2, "0x%02x, /* FC_FIXED_REPEAT */\n", RPC_FC_FIXED_REPEAT);
            print_file(file, 2, "0x%02x, /* FC_PAD */\n", RPC_FC_PAD);
            print_file(file, 2, "NdrFcShort(0x%x), /* Iterations = %d */\n", type_array_get_dim(type), type_array_get_dim(type));
            print_file(file, 2, "NdrFcShort(0x%x), /* Increment = %d */\n", increment_size, increment_size);
            print_file(file, 2, "NdrFcShort(0x%x), /* Offset to array = %d */\n", *offset_in_memory, *offset_in_memory);
            print_file(file, 2, "NdrFcShort(0x%x), /* Number of pointers = %d */\n", pointer_count, pointer_count);
            *typestring_offset += 10;

            pointer_count = write_pointer_description_offsets(
                file, attrs, type, &offset_of_array_pointer_mem,
                &offset_of_array_pointer_buf, typestring_offset);
        }
    }
    else if (is_struct(type->type))
    {
        const var_t *v;
        LIST_FOR_EACH_ENTRY( v, type_struct_get_fields(type), const var_t, entry )
        {
            if (offset_in_memory && offset_in_buffer)
            {
                size_t padding;
                align = 0;
                type_memsize(v->type, &align);
                padding = ROUNDING(*offset_in_memory, align);
                *offset_in_memory += padding;
                *offset_in_buffer += padding;
            }
            pointer_count += write_fixed_array_pointer_descriptions(
                file, v->attrs, v->type, offset_in_memory, offset_in_buffer,
                typestring_offset);
        }
    }
    else
    {
        if (offset_in_memory && offset_in_buffer)
        {
            size_t memsize;
            align = 0;
            memsize = type_memsize(type, &align);
            *offset_in_memory += memsize;
            /* increment these separately as in the case of conformant (varying)
             * structures these start at different values */
            *offset_in_buffer += memsize;
        }
    }

    return pointer_count;
}

/* Note: if file is NULL return value is number of pointers to write, else
 * it is the number of type format characters written */
static int write_conformant_array_pointer_descriptions(
    FILE *file, const attr_list_t *attrs, type_t *type,
    size_t offset_in_memory, unsigned int *typestring_offset)
{
    unsigned int align;
    int pointer_count = 0;

    if (is_conformant_array(type) && !type_array_has_variance(type))
    {
        unsigned int temp = 0;
        /* unfortunately, this needs to be done in two passes to avoid
         * writing out redundant FC_VARIABLE_REPEAT descriptions */
        pointer_count = write_pointer_description_offsets(
            NULL, attrs, type_array_get_element(type), NULL, NULL, &temp);
        if (pointer_count > 0)
        {
            unsigned int increment_size;
            size_t offset_of_array_pointer_mem = offset_in_memory;
            size_t offset_of_array_pointer_buf = offset_in_memory;

            align = 0;
            increment_size = type_memsize(type_array_get_element(type), &align);

            if (increment_size > USHRT_MAX)
                error("array size of %u bytes is too large\n", increment_size);

            print_file(file, 2, "0x%02x, /* FC_VARIABLE_REPEAT */\n", RPC_FC_VARIABLE_REPEAT);
            print_file(file, 2, "0x%02x, /* FC_FIXED_OFFSET */\n", RPC_FC_FIXED_OFFSET);
            print_file(file, 2, "NdrFcShort(0x%x), /* Increment = %d */\n", increment_size, increment_size);
            print_file(file, 2, "NdrFcShort(0x%x), /* Offset to array = %d */\n", offset_in_memory, offset_in_memory);
            print_file(file, 2, "NdrFcShort(0x%x), /* Number of pointers = %d */\n", pointer_count, pointer_count);
            *typestring_offset += 8;

            pointer_count = write_pointer_description_offsets(
                file, attrs, type_array_get_element(type),
                &offset_of_array_pointer_mem, &offset_of_array_pointer_buf,
                typestring_offset);
        }
    }

    return pointer_count;
}

/* Note: if file is NULL return value is number of pointers to write, else
 * it is the number of type format characters written */
static int write_varying_array_pointer_descriptions(
    FILE *file, const attr_list_t *attrs, type_t *type,
    size_t *offset_in_memory, size_t *offset_in_buffer,
    unsigned int *typestring_offset)
{
    unsigned int align;
    int pointer_count = 0;

    if (is_array(type) && type_array_has_variance(type))
    {
        unsigned int temp = 0;
        /* unfortunately, this needs to be done in two passes to avoid
         * writing out redundant FC_VARIABLE_REPEAT descriptions */
        pointer_count = write_pointer_description_offsets(
            NULL, attrs, type_array_get_element(type), NULL, NULL, &temp);
        if (pointer_count > 0)
        {
            unsigned int increment_size;

            align = 0;
            increment_size = type_memsize(type_array_get_element(type), &align);

            if (increment_size > USHRT_MAX)
                error("array size of %u bytes is too large\n", increment_size);

            print_file(file, 2, "0x%02x, /* FC_VARIABLE_REPEAT */\n", RPC_FC_VARIABLE_REPEAT);
            print_file(file, 2, "0x%02x, /* FC_VARIABLE_OFFSET */\n", RPC_FC_VARIABLE_OFFSET);
            print_file(file, 2, "NdrFcShort(0x%x), /* Increment = %d */\n", increment_size, increment_size);
            print_file(file, 2, "NdrFcShort(0x%x), /* Offset to array = %d */\n", *offset_in_memory, *offset_in_memory);
            print_file(file, 2, "NdrFcShort(0x%x), /* Number of pointers = %d */\n", pointer_count, pointer_count);
            *typestring_offset += 8;

            pointer_count = write_pointer_description_offsets(
                file, attrs, type, offset_in_memory,
                offset_in_buffer, typestring_offset);
        }
    }
    else if (is_struct(type->type))
    {
        const var_t *v;
        LIST_FOR_EACH_ENTRY( v, type_struct_get_fields(type), const var_t, entry )
        {
            if (offset_in_memory && offset_in_buffer)
            {
                size_t padding;

                if (is_array(v->type) && type_array_has_variance(v->type))
                {
                    *offset_in_buffer = ROUND_SIZE(*offset_in_buffer, 4);
                    /* skip over variance and offset in buffer */
                    *offset_in_buffer += 8;
                }

                align = 0;
                type_memsize(v->type, &align);
                padding = ROUNDING(*offset_in_memory, align);
                *offset_in_memory += padding;
                *offset_in_buffer += padding;
            }
            pointer_count += write_varying_array_pointer_descriptions(
                file, v->attrs, v->type, offset_in_memory, offset_in_buffer,
                typestring_offset);
        }
    }
    else
    {
        if (offset_in_memory && offset_in_buffer)
        {
            size_t memsize;
            align = 0;
            memsize = type_memsize(type, &align);
            *offset_in_memory += memsize;
            /* increment these separately as in the case of conformant (varying)
             * structures these start at different values */
            *offset_in_buffer += memsize;
        }
    }

    return pointer_count;
}

static void write_pointer_description(FILE *file, type_t *type,
                                      unsigned int *typestring_offset)
{
    size_t offset_in_buffer;
    size_t offset_in_memory;

    /* pass 1: search for single instance of a pointer (i.e. don't descend
     * into arrays) */
    if (!is_array(type))
    {
        offset_in_memory = 0;
        offset_in_buffer = 0;
        write_no_repeat_pointer_descriptions(
            file, type,
            &offset_in_memory, &offset_in_buffer, typestring_offset);
    }

    /* pass 2: search for pointers in fixed arrays */
    offset_in_memory = 0;
    offset_in_buffer = 0;
    write_fixed_array_pointer_descriptions(
        file, NULL, type,
        &offset_in_memory, &offset_in_buffer, typestring_offset);

    /* pass 3: search for pointers in conformant only arrays (but don't descend
     * into conformant varying or varying arrays) */
    if ((!type->declarray || !current_structure) && is_conformant_array(type))
        write_conformant_array_pointer_descriptions(
            file, NULL, type, 0, typestring_offset);
    else if (get_struct_type(type) == RPC_FC_CPSTRUCT)
    {
        unsigned int align = 0;
        type_t *carray = find_array_or_string_in_struct(type)->type;
        write_conformant_array_pointer_descriptions(
            file, NULL, carray,
            type_memsize(type, &align),
            typestring_offset);
    }

    /* pass 4: search for pointers in varying arrays */
    offset_in_memory = 0;
    offset_in_buffer = 0;
    write_varying_array_pointer_descriptions(
            file, NULL, type,
            &offset_in_memory, &offset_in_buffer, typestring_offset);
}

int is_declptr(const type_t *t)
{
  return is_ptr(t) || (is_conformant_array(t) && !t->declarray);
}

static size_t write_string_tfs(FILE *file, const attr_list_t *attrs,
                               type_t *type,
                               const char *name, unsigned int *typestring_offset)
{
    size_t start_offset;
    unsigned char rtype;

    if (is_declptr(type))
    {
        unsigned char flag = is_conformant_array(type) ? 0 : RPC_FC_P_SIMPLEPOINTER;
        int pointer_type = is_ptr(type) ? type->type : get_attrv(attrs, ATTR_POINTERTYPE);
        if (!pointer_type)
            pointer_type = RPC_FC_RP;
        print_start_tfs_comment(file, type, *typestring_offset);
        print_file(file, 2,"0x%x, 0x%x,\t/* %s%s */\n",
                   pointer_type, flag, string_of_type(pointer_type),
                   flag ? " [simple_pointer]" : "");
        *typestring_offset += 2;
        if (!flag)
        {
            print_file(file, 2, "NdrFcShort(0x2),\n");
            *typestring_offset += 2;
        }
    }

    start_offset = *typestring_offset;
    update_tfsoff(type, start_offset, file);

    if (is_array(type))
        rtype = type_array_get_element(type)->type;
    else
        rtype = type_pointer_get_ref(type)->type;

    if ((rtype != RPC_FC_BYTE) && (rtype != RPC_FC_CHAR) && (rtype != RPC_FC_WCHAR))
    {
        error("write_string_tfs: Unimplemented for type 0x%x of name: %s\n", rtype, name);
        return start_offset;
    }

    if (type->declarray && !is_conformant_array(type))
    {
        unsigned long dim = type_array_get_dim(type);

        /* FIXME: multi-dimensional array */
        if (0xffffuL < dim)
            error("array size for parameter %s exceeds %u bytes by %lu bytes\n",
                  name, 0xffffu, dim - 0xffffu);

        if (rtype == RPC_FC_CHAR)
            WRITE_FCTYPE(file, FC_CSTRING, *typestring_offset);
        else
            WRITE_FCTYPE(file, FC_WSTRING, *typestring_offset);
        print_file(file, 2, "0x%x, /* FC_PAD */\n", RPC_FC_PAD);
        *typestring_offset += 2;

        print_file(file, 2, "NdrFcShort(0x%x), /* %d */\n", dim, dim);
        *typestring_offset += 2;

        return start_offset;
    }
    else if (is_conformant_array(type))
    {
        unsigned int align = 0;

        if (rtype == RPC_FC_CHAR)
            WRITE_FCTYPE(file, FC_C_CSTRING, *typestring_offset);
        else
            WRITE_FCTYPE(file, FC_C_WSTRING, *typestring_offset);
        print_file(file, 2, "0x%x, /* FC_STRING_SIZED */\n", RPC_FC_STRING_SIZED);
        *typestring_offset += 2;

        *typestring_offset += write_conf_or_var_desc(
            file, current_structure,
            (type->declarray && current_structure
             ? type_memsize(current_structure, &align)
             : 0),
            type, type_array_get_conformance(type));

        return start_offset;
    }
    else
    {
        if (rtype == RPC_FC_WCHAR)
            WRITE_FCTYPE(file, FC_C_WSTRING, *typestring_offset);
        else
            WRITE_FCTYPE(file, FC_C_CSTRING, *typestring_offset);
        print_file(file, 2, "0x%x, /* FC_PAD */\n", RPC_FC_PAD);
        *typestring_offset += 2;

        return start_offset;
    }
}

static size_t write_array_tfs(FILE *file, const attr_list_t *attrs, type_t *type,
                              const char *name, unsigned int *typestring_offset)
{
    const expr_t *length_is = type_array_get_variance(type);
    const expr_t *size_is = type_array_get_conformance(type);
    unsigned int align = 0;
    size_t size;
    size_t start_offset;
    int real_type;
    int has_pointer;
    int pointer_type = get_attrv(attrs, ATTR_POINTERTYPE);
    unsigned int baseoff
        = type->declarray && current_structure
        ? type_memsize(current_structure, &align)
        : 0;

    if (!pointer_type)
        pointer_type = RPC_FC_RP;

    if (write_embedded_types(file, attrs, type_array_get_element(type), name, FALSE, typestring_offset))
        has_pointer = TRUE;
    else
        has_pointer = type_has_pointers(type_array_get_element(type));

    align = 0;
    size = type_memsize((is_conformant_array(type) ? type_array_get_element(type) : type), &align);
    real_type = get_array_type( type );

    start_offset = *typestring_offset;
    update_tfsoff(type, start_offset, file);
    print_start_tfs_comment(file, type, start_offset);
    print_file(file, 2, "0x%02x,\t/* %s */\n", real_type, string_of_type(real_type));
    print_file(file, 2, "0x%x,\t/* %d */\n", align - 1, align - 1);
    *typestring_offset += 2;

    align = 0;
    if (real_type != RPC_FC_BOGUS_ARRAY)
    {
        if (real_type == RPC_FC_LGFARRAY || real_type == RPC_FC_LGVARRAY)
        {
            print_file(file, 2, "NdrFcLong(0x%x),\t/* %lu */\n", size, size);
            *typestring_offset += 4;
        }
        else
        {
            print_file(file, 2, "NdrFcShort(0x%x),\t/* %lu */\n", size, size);
            *typestring_offset += 2;
        }

        if (is_conformant_array(type))
            *typestring_offset
                += write_conf_or_var_desc(file, current_structure, baseoff,
                                          type, size_is);

        if (real_type == RPC_FC_SMVARRAY || real_type == RPC_FC_LGVARRAY)
        {
            unsigned int elalign = 0;
            size_t elsize = type_memsize(type_array_get_element(type), &elalign);
            unsigned long dim = type_array_get_dim(type);

            if (real_type == RPC_FC_LGVARRAY)
            {
                print_file(file, 2, "NdrFcLong(0x%x),\t/* %lu */\n", dim, dim);
                *typestring_offset += 4;
            }
            else
            {
                print_file(file, 2, "NdrFcShort(0x%x),\t/* %lu */\n", dim, dim);
                *typestring_offset += 2;
            }

            print_file(file, 2, "NdrFcShort(0x%x),\t/* %lu */\n", elsize, elsize);
            *typestring_offset += 2;
        }

        if (length_is)
            *typestring_offset
                += write_conf_or_var_desc(file, current_structure, baseoff,
                                          type, length_is);

        if (has_pointer && (!type->declarray || !current_structure))
        {
            print_file(file, 2, "0x%x, /* FC_PP */\n", RPC_FC_PP);
            print_file(file, 2, "0x%x, /* FC_PAD */\n", RPC_FC_PAD);
            *typestring_offset += 2;
            write_pointer_description(file, type, typestring_offset);
            print_file(file, 2, "0x%x, /* FC_END */\n", RPC_FC_END);
            *typestring_offset += 1;
        }

        write_member_type(file, type, NULL, type_array_get_element(type), NULL, typestring_offset);
        write_end(file, typestring_offset);
    }
    else
    {
        unsigned int dim = size_is ? 0 : type_array_get_dim(type);
        print_file(file, 2, "NdrFcShort(0x%x),\t/* %u */\n", dim, dim);
        *typestring_offset += 2;
        *typestring_offset
            += write_conf_or_var_desc(file, current_structure, baseoff,
                                      type, size_is);
        *typestring_offset
            += write_conf_or_var_desc(file, current_structure, baseoff,
                                      type, length_is);
        write_member_type(file, type, NULL, type_array_get_element(type), NULL, typestring_offset);
        write_end(file, typestring_offset);
    }

    return start_offset;
}

static const var_t *find_array_or_string_in_struct(const type_t *type)
{
    const var_list_t *fields = type_struct_get_fields(type);
    const var_t *last_field;
    const type_t *ft;
    int real_type;

    if (!fields || list_empty(fields))
        return NULL;

    last_field = LIST_ENTRY( list_tail(fields), const var_t, entry );
    ft = last_field->type;

    if (ft->declarray && is_conformant_array(ft))
        return last_field;

    real_type = get_struct_type( type );
    if (real_type == RPC_FC_CSTRUCT || real_type == RPC_FC_CPSTRUCT || real_type == RPC_FC_CVSTRUCT)
        return find_array_or_string_in_struct(ft);
    else
        return NULL;
}

static void write_struct_members(FILE *file, const type_t *type,
                                 unsigned int *corroff, unsigned int *typestring_offset)
{
    const var_t *field;
    unsigned short offset = 0;
    int salign = -1;
    int padding;
    var_list_t *fields = type_struct_get_fields(type);

    if (fields) LIST_FOR_EACH_ENTRY( field, fields, const var_t, entry )
    {
        type_t *ft = field->type;
        if (!ft->declarray || !is_conformant_array(ft))
        {
            unsigned int align = 0;
            size_t size = type_memsize(ft, &align);
            if (salign == -1)
                salign = align;
            if ((align - 1) & offset)
            {
                unsigned char fc = 0;
                switch (align)
                {
                case 4:
                    fc = RPC_FC_ALIGNM4;
                    break;
                case 8:
                    fc = RPC_FC_ALIGNM8;
                    break;
                default:
                    error("write_struct_members: cannot align type %d\n", ft->type);
                }
                print_file(file, 2, "0x%x,\t/* %s */\n", fc, string_of_type(fc));
                offset = ROUND_SIZE(offset, align);
                *typestring_offset += 1;
            }
            write_member_type(file, type, field->attrs, field->type, corroff,
                              typestring_offset);
            offset += size;
        }
    }

    padding = ROUNDING(offset, salign);
    if (padding)
    {
        print_file(file, 2, "0x%x,\t/* FC_STRUCTPAD%d */\n",
                   RPC_FC_STRUCTPAD1 + padding - 1,
                   padding);
        *typestring_offset += 1;
    }

    write_end(file, typestring_offset);
}

static size_t write_struct_tfs(FILE *file, type_t *type,
                               const char *name, unsigned int *tfsoff)
{
    const type_t *save_current_structure = current_structure;
    unsigned int total_size;
    const var_t *array;
    size_t start_offset;
    size_t array_offset;
    int has_pointers = 0;
    unsigned int align = 0;
    unsigned int corroff;
    var_t *f;
    int real_type = get_struct_type( type );
    var_list_t *fields = type_struct_get_fields(type);

    guard_rec(type);
    current_structure = type;

    total_size = type_memsize(type, &align);
    if (total_size > USHRT_MAX)
        error("structure size for %s exceeds %d bytes by %d bytes\n",
              name, USHRT_MAX, total_size - USHRT_MAX);

    if (fields) LIST_FOR_EACH_ENTRY(f, fields, var_t, entry)
        has_pointers |= write_embedded_types(file, f->attrs, f->type, f->name,
                                             FALSE, tfsoff);
    if (!has_pointers) has_pointers = type_has_pointers(type);

    array = find_array_or_string_in_struct(type);
    if (array && !processed(array->type))
        array_offset
            = is_string_type(array->attrs, array->type)
            ? write_string_tfs(file, array->attrs, array->type, array->name, tfsoff)
            : write_array_tfs(file, array->attrs, array->type, array->name, tfsoff);

    corroff = *tfsoff;
    write_descriptors(file, type, tfsoff);

    start_offset = *tfsoff;
    update_tfsoff(type, start_offset, file);
    print_start_tfs_comment(file, type, start_offset);
    print_file(file, 2, "0x%x,\t/* %s */\n", real_type, string_of_type(real_type));
    print_file(file, 2, "0x%x,\t/* %d */\n", align - 1, align - 1);
    print_file(file, 2, "NdrFcShort(0x%x),\t/* %d */\n", total_size, total_size);
    *tfsoff += 4;

    if (array)
    {
        unsigned int absoff = array->type->typestring_offset;
        short reloff = absoff - *tfsoff;
        print_file(file, 2, "NdrFcShort(0x%hx),\t/* Offset= %hd (%lu) */\n",
                   reloff, reloff, absoff);
        *tfsoff += 2;
    }
    else if (real_type == RPC_FC_BOGUS_STRUCT)
    {
        print_file(file, 2, "NdrFcShort(0x0),\n");
        *tfsoff += 2;
    }

    if (real_type == RPC_FC_BOGUS_STRUCT)
    {
        /* On the sizing pass, type->ptrdesc may be zero, but it's ok as
           nothing is written to file yet.  On the actual writing pass,
           this will have been updated.  */
        unsigned int absoff = type->ptrdesc ? type->ptrdesc : *tfsoff;
        int reloff = absoff - *tfsoff;
        assert( reloff >= 0 );
        print_file(file, 2, "NdrFcShort(0x%x),\t/* Offset= %d (%u) */\n",
                   reloff, reloff, absoff);
        *tfsoff += 2;
    }
    else if ((real_type == RPC_FC_PSTRUCT) ||
             (real_type == RPC_FC_CPSTRUCT) ||
             (real_type == RPC_FC_CVSTRUCT && has_pointers))
    {
        print_file(file, 2, "0x%x, /* FC_PP */\n", RPC_FC_PP);
        print_file(file, 2, "0x%x, /* FC_PAD */\n", RPC_FC_PAD);
        *tfsoff += 2;
        write_pointer_description(file, type, tfsoff);
        print_file(file, 2, "0x%x, /* FC_END */\n", RPC_FC_END);
        *tfsoff += 1;
    }

    write_struct_members(file, type, &corroff, tfsoff);

    if (real_type == RPC_FC_BOGUS_STRUCT)
    {
        const var_t *f;

        type->ptrdesc = *tfsoff;
        if (fields) LIST_FOR_EACH_ENTRY(f, fields, const var_t, entry)
        {
            type_t *ft = f->type;
            if (is_ptr(ft))
            {
                if (is_string_type(f->attrs, ft))
                    write_string_tfs(file, f->attrs, ft, f->name, tfsoff);
                else
                    write_pointer_tfs(file, ft, tfsoff);
            }
            else if (!ft->declarray && is_conformant_array(ft))
            {
                unsigned int absoff = ft->typestring_offset;
                short reloff = absoff - (*tfsoff + 2);
                int ptr_type = get_attrv(f->attrs, ATTR_POINTERTYPE);
                /* FIXME: We need to store pointer attributes for arrays
                   so we don't lose pointer_default info.  */
                if (ptr_type == 0)
                    ptr_type = RPC_FC_UP;
                print_file(file, 0, "/* %d */\n", *tfsoff);
                print_file(file, 2, "0x%x, 0x0,\t/* %s */\n", ptr_type,
                           string_of_type(ptr_type));
                print_file(file, 2, "NdrFcShort(0x%hx),\t/* Offset= %hd (%u) */\n",
                           reloff, reloff, absoff);
                *tfsoff += 4;
            }
        }
        if (type->ptrdesc == *tfsoff)
            type->ptrdesc = 0;
    }

    current_structure = save_current_structure;
    return start_offset;
}

static size_t write_pointer_only_tfs(FILE *file, const attr_list_t *attrs, int pointer_type,
                                     unsigned char flags, size_t offset,
                                     unsigned int *typeformat_offset)
{
    size_t start_offset = *typeformat_offset;
    short reloff = offset - (*typeformat_offset + 2);
    int in_attr, out_attr;
    in_attr = is_attr(attrs, ATTR_IN);
    out_attr = is_attr(attrs, ATTR_OUT);
    if (!in_attr && !out_attr) in_attr = 1;

    if (out_attr && !in_attr && pointer_type == RPC_FC_RP)
        flags |= 0x04;

    print_file(file, 2, "0x%x, 0x%x,\t\t/* %s",
               pointer_type,
               flags,
               string_of_type(pointer_type));
    if (file)
    {
        if (flags & 0x04)
            fprintf(file, " [allocated_on_stack]");
        if (flags & 0x10)
            fprintf(file, " [pointer_deref]");
        fprintf(file, " */\n");
    }

    print_file(file, 2, "NdrFcShort(0x%x),\t/* %d */\n", reloff, offset);
    *typeformat_offset += 4;

    return start_offset;
}

static void write_branch_type(FILE *file, const type_t *t, unsigned int *tfsoff)
{
    if (t == NULL)
    {
        print_file(file, 2, "NdrFcShort(0x0),\t/* No type */\n");
    }
    else if (is_base_type(t->type))
    {
        print_file(file, 2, "NdrFcShort(0x80%02x),\t/* Simple arm type: %s */\n",
                   t->type, string_of_type(t->type));
    }
    else if (t->typestring_offset)
    {
        short reloff = t->typestring_offset - *tfsoff;
        print_file(file, 2, "NdrFcShort(0x%x),\t/* Offset= %d (%d) */\n",
                   reloff, reloff, t->typestring_offset);
    }
    else
        error("write_branch_type: type unimplemented (0x%x)\n", t->type);

    *tfsoff += 2;
}

static size_t write_union_tfs(FILE *file, type_t *type, unsigned int *tfsoff)
{
    unsigned int align = 0;
    unsigned int start_offset;
    size_t size = type_memsize(type, &align);
    var_list_t *fields;
    size_t nbranch = 0;
    type_t *deftype = NULL;
    short nodeftype = 0xffff;
    var_t *f;

    guard_rec(type);

    fields = type_union_get_cases(type);

    if (fields) LIST_FOR_EACH_ENTRY(f, fields, var_t, entry)
    {
        expr_list_t *cases = get_attrp(f->attrs, ATTR_CASE);
        if (cases)
            nbranch += list_count(cases);
        if (f->type)
            write_embedded_types(file, f->attrs, f->type, f->name, TRUE, tfsoff);
    }

    start_offset = *tfsoff;
    update_tfsoff(type, start_offset, file);
    print_start_tfs_comment(file, type, start_offset);
    if (type->type == RPC_FC_ENCAPSULATED_UNION)
    {
        const var_t *sv = type_union_get_switch_value(type);
        const type_t *st = sv->type;

        switch (st->type)
        {
        case RPC_FC_CHAR:
        case RPC_FC_SMALL:
        case RPC_FC_USMALL:
        case RPC_FC_SHORT:
        case RPC_FC_USHORT:
        case RPC_FC_LONG:
        case RPC_FC_ULONG:
        case RPC_FC_ENUM16:
        case RPC_FC_ENUM32:
            print_file(file, 2, "0x%x,\t/* %s */\n", type->type, string_of_type(type->type));
            print_file(file, 2, "0x%x,\t/* Switch type= %s */\n",
                       0x40 | st->type, string_of_type(st->type));
            *tfsoff += 2;
            break;
        default:
            error("union switch type must be an integer, char, or enum\n");
        }
    }
    else if (is_attr(type->attrs, ATTR_SWITCHTYPE))
    {
        static const expr_t dummy_expr;  /* FIXME */
        const type_t *st = get_attrp(type->attrs, ATTR_SWITCHTYPE);

        switch (st->type)
        {
        case RPC_FC_CHAR:
        case RPC_FC_SMALL:
        case RPC_FC_USMALL:
        case RPC_FC_SHORT:
        case RPC_FC_USHORT:
        case RPC_FC_LONG:
        case RPC_FC_ULONG:
        case RPC_FC_ENUM16:
        case RPC_FC_ENUM32:
            print_file(file, 2, "0x%x,\t/* %s */\n", type->type, string_of_type(type->type));
            print_file(file, 2, "0x%x,\t/* Switch type= %s */\n",
                       st->type, string_of_type(st->type));
            *tfsoff += 2;
            break;
        default:
            error("union switch type must be an integer, char, or enum\n");
        }

        *tfsoff += write_conf_or_var_desc(file, NULL, *tfsoff, st, &dummy_expr );
    }

    print_file(file, 2, "NdrFcShort(0x%x),\t/* %d */\n", size, size);
    print_file(file, 2, "NdrFcShort(0x%x),\t/* %d */\n", nbranch, nbranch);
    *tfsoff += 4;

    if (fields) LIST_FOR_EACH_ENTRY(f, fields, var_t, entry)
    {
        type_t *ft = f->type;
        expr_list_t *cases = get_attrp(f->attrs, ATTR_CASE);
        int deflt = is_attr(f->attrs, ATTR_DEFAULT);
        expr_t *c;

        if (cases == NULL && !deflt)
            error("union field %s with neither case nor default attribute\n", f->name);

        if (cases) LIST_FOR_EACH_ENTRY(c, cases, expr_t, entry)
        {
            /* MIDL doesn't check for duplicate cases, even though that seems
               like a reasonable thing to do, it just dumps them to the TFS
               like we're going to do here.  */
            print_file(file, 2, "NdrFcLong(0x%x),\t/* %d */\n", c->cval, c->cval);
            *tfsoff += 4;
            write_branch_type(file, ft, tfsoff);
        }

        /* MIDL allows multiple default branches, even though that seems
           illogical, it just chooses the last one, which is what we will
           do.  */
        if (deflt)
        {
            deftype = ft;
            nodeftype = 0;
        }
    }

    if (deftype)
    {
        write_branch_type(file, deftype, tfsoff);
    }
    else
    {
        print_file(file, 2, "NdrFcShort(0x%x),\n", nodeftype);
        *tfsoff += 2;
    }

    return start_offset;
}

static size_t write_ip_tfs(FILE *file, const attr_list_t *attrs, type_t *type,
                           unsigned int *typeformat_offset)
{
    size_t i;
    size_t start_offset = *typeformat_offset;
    expr_t *iid = get_attrp(attrs, ATTR_IIDIS);

    if (iid)
    {
        print_file(file, 2, "0x2f,  /* FC_IP */\n");
        print_file(file, 2, "0x5c,  /* FC_PAD */\n");
        *typeformat_offset
            += write_conf_or_var_desc(file, NULL, 0, type, iid) + 2;
    }
    else
    {
        const type_t *base = is_ptr(type) ? type_pointer_get_ref(type) : type;
        const UUID *uuid = get_attrp(base->attrs, ATTR_UUID);

        if (! uuid)
            error("%s: interface %s missing UUID\n", __FUNCTION__, base->name);

        update_tfsoff(type, start_offset, file);
        print_start_tfs_comment(file, type, start_offset);
        print_file(file, 2, "0x2f,\t/* FC_IP */\n");
        print_file(file, 2, "0x5a,\t/* FC_CONSTANT_IID */\n");
        print_file(file, 2, "NdrFcLong(0x%08lx),\n", uuid->Data1);
        print_file(file, 2, "NdrFcShort(0x%04x),\n", uuid->Data2);
        print_file(file, 2, "NdrFcShort(0x%04x),\n", uuid->Data3);
        for (i = 0; i < 8; ++i)
            print_file(file, 2, "0x%02x,\n", uuid->Data4[i]);

        if (file)
            fprintf(file, "\n");

        *typeformat_offset += 18;
    }
    return start_offset;
}

static size_t write_contexthandle_tfs(FILE *file, const type_t *type,
                                      const var_t *var,
                                      unsigned int *typeformat_offset)
{
    size_t start_offset = *typeformat_offset;
    unsigned char flags = 0;

    if (is_attr(current_iface->attrs, ATTR_STRICTCONTEXTHANDLE))
        flags |= NDR_STRICT_CONTEXT_HANDLE;

    if (is_ptr(type))
        flags |= 0x80;
    if (is_attr(var->attrs, ATTR_IN))
    {
        flags |= 0x40;
        if (!is_attr(var->attrs, ATTR_OUT))
            flags |= NDR_CONTEXT_HANDLE_CANNOT_BE_NULL;
    }
    if (is_attr(var->attrs, ATTR_OUT))
        flags |= 0x20;

    WRITE_FCTYPE(file, FC_BIND_CONTEXT, *typeformat_offset);
    print_file(file, 2, "0x%x,\t/* Context flags: ", flags);
    /* return and can't be null values overlap */
    if (((flags & 0x21) != 0x21) && (flags & NDR_CONTEXT_HANDLE_CANNOT_BE_NULL))
        print_file(file, 0, "can't be null, ");
    if (flags & NDR_CONTEXT_HANDLE_SERIALIZE)
        print_file(file, 0, "serialize, ");
    if (flags & NDR_CONTEXT_HANDLE_NO_SERIALIZE)
        print_file(file, 0, "no serialize, ");
    if (flags & NDR_STRICT_CONTEXT_HANDLE)
        print_file(file, 0, "strict, ");
    if ((flags & 0x21) == 0x20)
        print_file(file, 0, "out, ");
    if ((flags & 0x21) == 0x21)
        print_file(file, 0, "return, ");
    if (flags & 0x40)
        print_file(file, 0, "in, ");
    if (flags & 0x80)
        print_file(file, 0, "via ptr, ");
    print_file(file, 0, "*/\n");
    print_file(file, 2, "0, /* FIXME: rundown routine index*/\n");
    print_file(file, 2, "0, /* FIXME: param num */\n");
    *typeformat_offset += 4;

    return start_offset;
}

static size_t write_typeformatstring_var(FILE *file, int indent, const var_t *func,
                                         type_t *type, const var_t *var,
                                         unsigned int *typeformat_offset)
{
    size_t offset;

    if (is_context_handle(type))
        return write_contexthandle_tfs(file, type, var, typeformat_offset);

    if (is_user_type(type))
    {
        write_user_tfs(file, type, typeformat_offset);
        return type->typestring_offset;
    }

    if (is_string_type(var->attrs, type))
        return write_string_tfs(file, var->attrs, type, var->name, typeformat_offset);

    if (is_array(type))
    {
        int ptr_type;
        size_t off;
        off = write_array_tfs(file, var->attrs, type, var->name, typeformat_offset);
        ptr_type = get_attrv(var->attrs, ATTR_POINTERTYPE);
        /* Top level pointers to conformant arrays may be handled specially
           since we can bypass the pointer, but if the array is buried
           beneath another pointer (e.g., "[size_is(,n)] int **p" then we
           always need to write the pointer.  */
        if (!ptr_type && var->type != type)
          /* FIXME:  This should use pointer_default, but the information
             isn't kept around for arrays.  */
          ptr_type = RPC_FC_UP;
        if (ptr_type && ptr_type != RPC_FC_RP)
        {
            unsigned int absoff = type->typestring_offset;
            short reloff = absoff - (*typeformat_offset + 2);
            off = *typeformat_offset;
            print_file(file, 0, "/* %d */\n", off);
            print_file(file, 2, "0x%x, 0x0,\t/* %s */\n", ptr_type,
                       string_of_type(ptr_type));
            print_file(file, 2, "NdrFcShort(0x%hx),\t/* Offset= %hd (%u) */\n",
                       reloff, reloff, absoff);
            *typeformat_offset += 4;
        }
        return off;
    }

    if (!is_ptr(type))
    {
        /* basic types don't need a type format string */
        if (is_base_type(type->type))
            return 0;

        if (processed(type)) return type->typestring_offset;

        switch (type->type)
        {
        case RPC_FC_STRUCT:
        case RPC_FC_PSTRUCT:
        case RPC_FC_CSTRUCT:
        case RPC_FC_CPSTRUCT:
        case RPC_FC_CVSTRUCT:
        case RPC_FC_BOGUS_STRUCT:
            return write_struct_tfs(file, type, var->name, typeformat_offset);
        case RPC_FC_ENCAPSULATED_UNION:
        case RPC_FC_NON_ENCAPSULATED_UNION:
            return write_union_tfs(file, type, typeformat_offset);
        case RPC_FC_IGNORE:
        case RPC_FC_BIND_PRIMITIVE:
            /* nothing to do */
            return 0;
        default:
            error("write_typeformatstring_var: Unsupported type 0x%x for variable %s\n", type->type, var->name);
        }
    }
    else if (last_ptr(type))
    {
        size_t start_offset = *typeformat_offset;
        int in_attr = is_attr(var->attrs, ATTR_IN);
        int out_attr = is_attr(var->attrs, ATTR_OUT);
        const type_t *base = type_pointer_get_ref(type);

        if (base->type == RPC_FC_IP
            || (base->type == 0
                && is_attr(var->attrs, ATTR_IIDIS)))
        {
            return write_ip_tfs(file, var->attrs, type, typeformat_offset);
        }

        /* special case for pointers to base types */
        if (is_base_type(base->type))
        {
            print_file(file, indent, "0x%x, 0x%x,    /* %s %s[simple_pointer] */\n",
                       type->type, (!in_attr && out_attr) ? 0x0C : 0x08,
                       string_of_type(type->type),
                       (!in_attr && out_attr) ? "[allocated_on_stack] " : "");
            print_file(file, indent, "0x%02x,    /* %s */\n", base->type, string_of_type(base->type));
            print_file(file, indent, "0x5c,          /* FC_PAD */\n");
            *typeformat_offset += 4;
            return start_offset;
        }
    }

    assert(is_ptr(type));

    offset = write_typeformatstring_var(file, indent, func,
                                        type_pointer_get_ref(type), var,
                                        typeformat_offset);
    if (file)
        fprintf(file, "/* %2u */\n", *typeformat_offset);
    return write_pointer_only_tfs(file, var->attrs, type->type,
                           !last_ptr(type) ? 0x10 : 0,
                           offset, typeformat_offset);
}

static int write_embedded_types(FILE *file, const attr_list_t *attrs, type_t *type,
                                const char *name, int write_ptr, unsigned int *tfsoff)
{
    int retmask = 0;

    if (is_user_type(type))
    {
        write_user_tfs(file, type, tfsoff);
    }
    else if (is_string_type(attrs, type))
    {
        write_string_tfs(file, attrs, type, name, tfsoff);
    }
    else if (is_ptr(type))
    {
        type_t *ref = type_pointer_get_ref(type);

        if (ref->type == RPC_FC_IP
            || (ref->type == 0
                && is_attr(attrs, ATTR_IIDIS)))
        {
            write_ip_tfs(file, attrs, type, tfsoff);
        }
        else
        {
            if (!processed(ref) && !is_base_type(ref->type))
                retmask |= write_embedded_types(file, NULL, ref, name, TRUE, tfsoff);

            if (write_ptr)
                write_pointer_tfs(file, type, tfsoff);

            retmask |= 1;
        }
    }
    else if (type->declarray && is_conformant_array(type))
        ;    /* conformant arrays and strings are handled specially */
    else if (is_array(type))
    {
        write_array_tfs(file, attrs, type, name, tfsoff);
        if (is_conformant_array(type))
            retmask |= 1;
    }
    else if (is_struct(type->type))
    {
        if (!processed(type))
            write_struct_tfs(file, type, name, tfsoff);
    }
    else if (is_union(type->type))
    {
        if (!processed(type))
            write_union_tfs(file, type, tfsoff);
    }
    else if (!is_base_type(type->type))
        error("write_embedded_types: unknown embedded type for %s (0x%x)\n",
              name, type->type);

    return retmask;
}

static size_t process_tfs_stmts(FILE *file, const statement_list_t *stmts,
                                type_pred_t pred, unsigned int *typeformat_offset)
{
    const var_t *var;
    const statement_t *stmt;

    if (stmts) LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
    {
        const type_t *iface;
        const statement_t *stmt_func;

        if (stmt->type == STMT_LIBRARY)
        {
            process_tfs_stmts(file, stmt->u.lib->stmts, pred, typeformat_offset);
            continue;
        }
        else if (stmt->type != STMT_TYPE || stmt->u.type->type != RPC_FC_IP)
            continue;

        iface = stmt->u.type;
        if (!pred(iface))
            continue;

        current_iface = iface;
        STATEMENTS_FOR_EACH_FUNC( stmt_func, type_iface_get_stmts(iface) )
        {
            const var_t *func = stmt_func->u.var;
                if (is_local(func->attrs)) continue;

                if (!is_void(type_function_get_rettype(func->type)))
                {
                    var_t v = *func;
                    v.type = type_function_get_rettype(func->type);
                    update_tfsoff(type_function_get_rettype(func->type),
                                  write_typeformatstring_var(
                                      file, 2, NULL,
                                      type_function_get_rettype(func->type),
                                      &v, typeformat_offset),
                                  file);
                }

                current_func = func;
                if (type_get_function_args(func->type))
                    LIST_FOR_EACH_ENTRY( var, type_get_function_args(func->type), const var_t, entry )
                        update_tfsoff(
                            var->type,
                            write_typeformatstring_var(
                                file, 2, func, var->type, var,
                                typeformat_offset),
                            file);
        }
    }

    return *typeformat_offset + 1;
}

static size_t process_tfs(FILE *file, const statement_list_t *stmts, type_pred_t pred)
{
    unsigned int typeformat_offset = 2;

    return process_tfs_stmts(file, stmts, pred, &typeformat_offset);
}


void write_typeformatstring(FILE *file, const statement_list_t *stmts, type_pred_t pred)
{
    int indent = 0;

    print_file(file, indent, "static const MIDL_TYPE_FORMAT_STRING __MIDL_TypeFormatString =\n");
    print_file(file, indent, "{\n");
    indent++;
    print_file(file, indent, "0,\n");
    print_file(file, indent, "{\n");
    indent++;
    print_file(file, indent, "NdrFcShort(0x0),\n");

    set_all_tfswrite(TRUE);
    process_tfs(file, stmts, pred);

    print_file(file, indent, "0x0\n");
    indent--;
    print_file(file, indent, "}\n");
    indent--;
    print_file(file, indent, "};\n");
    print_file(file, indent, "\n");
}

static unsigned int get_required_buffer_size_type(
    const type_t *type, const char *name, unsigned int *alignment)
{
    const char *uname;
    const type_t *utype;

    *alignment = 0;
    if ((utype = get_user_type(type, &uname)))
    {
        return get_required_buffer_size_type(utype, uname, alignment);
    }
    else
    {
        switch (get_struct_type(type))
        {
        case RPC_FC_BYTE:
        case RPC_FC_CHAR:
        case RPC_FC_USMALL:
        case RPC_FC_SMALL:
            *alignment = 4;
            return 1;

        case RPC_FC_WCHAR:
        case RPC_FC_USHORT:
        case RPC_FC_SHORT:
        case RPC_FC_ENUM16:
            *alignment = 4;
            return 2;

        case RPC_FC_ULONG:
        case RPC_FC_LONG:
        case RPC_FC_ENUM32:
        case RPC_FC_FLOAT:
        case RPC_FC_ERROR_STATUS_T:
            *alignment = 4;
            return 4;

        case RPC_FC_HYPER:
        case RPC_FC_DOUBLE:
            *alignment = 8;
            return 8;

        case RPC_FC_IGNORE:
        case RPC_FC_BIND_PRIMITIVE:
            return 0;

        case RPC_FC_STRUCT:
            if (!type_struct_get_fields(type)) return 0;
            return fields_memsize(type_struct_get_fields(type), alignment);

        case RPC_FC_RP:
        {
            const type_t *ref = type_pointer_get_ref(type);
            return is_base_type( ref->type ) || get_struct_type(ref) == RPC_FC_STRUCT ?
                get_required_buffer_size_type( ref, name, alignment ) : 0;
        }

        case RPC_FC_SMFARRAY:
        case RPC_FC_LGFARRAY:
            return type_array_get_dim(type) * get_required_buffer_size_type(type_array_get_element(type), name, alignment);

        default:
            return 0;
        }
    }
}

static unsigned int get_required_buffer_size(const var_t *var, unsigned int *alignment, enum pass pass)
{
    int in_attr = is_attr(var->attrs, ATTR_IN);
    int out_attr = is_attr(var->attrs, ATTR_OUT);

    if (!in_attr && !out_attr)
        in_attr = 1;

    *alignment = 0;

    if ((pass == PASS_IN && in_attr) || (pass == PASS_OUT && out_attr) ||
        pass == PASS_RETURN)
    {
        if (is_ptrchain_attr(var, ATTR_CONTEXTHANDLE))
        {
            *alignment = 4;
            return 20;
        }

        if (!is_string_type(var->attrs, var->type))
            return get_required_buffer_size_type(var->type, var->name,
                                                 alignment);
    }
    return 0;
}

static unsigned int get_function_buffer_size( const var_t *func, enum pass pass )
{
    const var_t *var;
    unsigned int total_size = 0, alignment;

    if (type_get_function_args(func->type))
    {
        LIST_FOR_EACH_ENTRY( var, type_get_function_args(func->type), const var_t, entry )
        {
            total_size += get_required_buffer_size(var, &alignment, pass);
            total_size += alignment;
        }
    }

    if (pass == PASS_OUT && !is_void(type_function_get_rettype(func->type)))
    {
        var_t v = *func;
        v.type = type_function_get_rettype(func->type);
        total_size += get_required_buffer_size(&v, &alignment, PASS_RETURN);
        total_size += alignment;
    }
    return total_size;
}

static void print_phase_function(FILE *file, int indent, const char *type,
                                 const char *local_var_prefix, enum remoting_phase phase,
                                 const var_t *var, unsigned int type_offset)
{
    const char *function;
    switch (phase)
    {
    case PHASE_BUFFERSIZE:
        function = "BufferSize";
        break;
    case PHASE_MARSHAL:
        function = "Marshall";
        break;
    case PHASE_UNMARSHAL:
        function = "Unmarshall";
        break;
    case PHASE_FREE:
        function = "Free";
        break;
    default:
        assert(0);
        return;
    }

    print_file(file, indent, "Ndr%s%s(\n", type, function);
    indent++;
    print_file(file, indent, "&__frame->_StubMsg,\n");
    print_file(file, indent, "%s%s%s%s%s,\n",
               (phase == PHASE_UNMARSHAL) ? "(unsigned char **)" : "(unsigned char *)",
               (phase == PHASE_UNMARSHAL || decl_indirect(var->type)) ? "&" : "",
               local_var_prefix,
               (phase == PHASE_UNMARSHAL && decl_indirect(var->type)) ? "_p_" : "",
               var->name);
    print_file(file, indent, "(PFORMAT_STRING)&__MIDL_TypeFormatString.Format[%d]%s\n",
               type_offset, (phase == PHASE_UNMARSHAL) ? "," : ");");
    if (phase == PHASE_UNMARSHAL)
        print_file(file, indent, "0);\n");
    indent--;
}

void print_phase_basetype(FILE *file, int indent, const char *local_var_prefix,
                          enum remoting_phase phase, enum pass pass, const var_t *var,
                          const char *varname)
{
    type_t *type = var->type;
    unsigned int size;
    unsigned int alignment = 0;
    unsigned char rtype;

    /* no work to do for other phases, buffer sizing is done elsewhere */
    if (phase != PHASE_MARSHAL && phase != PHASE_UNMARSHAL)
        return;

    rtype = is_ptr(type) ? type_pointer_get_ref(type)->type : type->type;

    switch (rtype)
    {
        case RPC_FC_BYTE:
        case RPC_FC_CHAR:
        case RPC_FC_SMALL:
        case RPC_FC_USMALL:
            size = 1;
            alignment = 1;
            break;

        case RPC_FC_WCHAR:
        case RPC_FC_USHORT:
        case RPC_FC_SHORT:
        case RPC_FC_ENUM16:
            size = 2;
            alignment = 2;
            break;

        case RPC_FC_ULONG:
        case RPC_FC_LONG:
        case RPC_FC_ENUM32:
        case RPC_FC_FLOAT:
        case RPC_FC_ERROR_STATUS_T:
            size = 4;
            alignment = 4;
            break;

        case RPC_FC_HYPER:
        case RPC_FC_DOUBLE:
            size = 8;
            alignment = 8;
            break;

        case RPC_FC_IGNORE:
        case RPC_FC_BIND_PRIMITIVE:
            /* no marshalling needed */
            return;

        default:
            error("print_phase_basetype: Unsupported type: %s (0x%02x, ptr_level: 0)\n", var->name, rtype);
            size = 0;
    }

    if (phase == PHASE_MARSHAL)
        print_file(file, indent, "MIDL_memset(__frame->_StubMsg.Buffer, 0, (0x%x - (ULONG_PTR)__frame->_StubMsg.Buffer) & 0x%x);\n", alignment, alignment - 1);
    print_file(file, indent, "__frame->_StubMsg.Buffer = (unsigned char *)(((ULONG_PTR)__frame->_StubMsg.Buffer + %u) & ~0x%x);\n",
                alignment - 1, alignment - 1);

    if (phase == PHASE_MARSHAL)
    {
        print_file(file, indent, "*(");
        write_type_decl(file, is_ptr(type) ? type_pointer_get_ref(type) : type, NULL);
        if (is_ptr(type))
            fprintf(file, " *)__frame->_StubMsg.Buffer = *");
        else
            fprintf(file, " *)__frame->_StubMsg.Buffer = ");
        fprintf(file, "%s%s", local_var_prefix, varname);
        fprintf(file, ";\n");
    }
    else if (phase == PHASE_UNMARSHAL)
    {
        print_file(file, indent, "if (__frame->_StubMsg.Buffer + sizeof(");
        write_type_decl(file, is_ptr(type) ? type_pointer_get_ref(type) : type, NULL);
        fprintf(file, ") > __frame->_StubMsg.BufferEnd)\n");
        print_file(file, indent, "{\n");
        print_file(file, indent + 1, "RpcRaiseException(RPC_X_BAD_STUB_DATA);\n");
        print_file(file, indent, "}\n");
        if (pass == PASS_IN || pass == PASS_RETURN)
            print_file(file, indent, "");
        else
            print_file(file, indent, "*");
        fprintf(file, "%s%s", local_var_prefix, varname);
        if (pass == PASS_IN && is_ptr(type))
            fprintf(file, " = (");
        else
            fprintf(file, " = *(");
        write_type_decl(file, is_ptr(type) ? type_pointer_get_ref(type) : type, NULL);
        fprintf(file, " *)__frame->_StubMsg.Buffer;\n");
    }

    print_file(file, indent, "__frame->_StubMsg.Buffer += sizeof(");
    write_type_decl(file, is_ptr(type) ? type_pointer_get_ref(type) : type, NULL);
    fprintf(file, ");\n");
}

/* returns whether the MaxCount, Offset or ActualCount members need to be
 * filled in for the specified phase */
static inline int is_conformance_needed_for_phase(enum remoting_phase phase)
{
    return (phase != PHASE_UNMARSHAL);
}

expr_t *get_size_is_expr(const type_t *t, const char *name)
{
    expr_t *x = NULL;

    for ( ; is_array(t); t = type_array_get_element(t))
        if (type_array_has_conformance(t))
        {
            if (!x)
                x = type_array_get_conformance(t);
            else
                error("%s: multidimensional conformant"
                      " arrays not supported at the top level\n",
                      name);
        }

    return x;
}

static void write_parameter_conf_or_var_exprs(FILE *file, int indent, const char *local_var_prefix,
                                              enum remoting_phase phase, const var_t *var)
{
    const type_t *type = var->type;
    /* get fundamental type for the argument */
    for (;;)
    {
        if (is_attr(type->attrs, ATTR_WIREMARSHAL))
            break;
        else if (is_attr(type->attrs, ATTR_CONTEXTHANDLE))
            break;
        else if (is_array(type) || is_string_type(var->attrs, type))
        {
            if (is_conformance_needed_for_phase(phase) && is_array(type))
            {
                if (type_array_has_conformance(type))
                {
                    print_file(file, indent, "__frame->_StubMsg.MaxCount = (ULONG_PTR)");
                    write_expr(file, type_array_get_conformance(type), 1, 1, NULL, NULL, local_var_prefix);
                    fprintf(file, ";\n\n");
                }
                if (type_array_has_variance(type))
                {
                    print_file(file, indent, "__frame->_StubMsg.Offset = 0;\n"); /* FIXME */
                    print_file(file, indent, "__frame->_StubMsg.ActualCount = (ULONG_PTR)");
                    write_expr(file, type_array_get_variance(type), 1, 1, NULL, NULL, local_var_prefix);
                    fprintf(file, ";\n\n");
                }
            }
            break;
        }
        else if (type->type == RPC_FC_NON_ENCAPSULATED_UNION)
        {
            if (is_conformance_needed_for_phase(phase))
            {
                print_file(file, indent, "__frame->_StubMsg.MaxCount = (ULONG_PTR)");
                write_expr(file, get_attrp(var->attrs, ATTR_SWITCHIS), 1, 1, NULL, NULL, local_var_prefix);
                fprintf(file, ";\n\n");
            }
            break;
        }
        else if (type->type == RPC_FC_IP || is_void(type))
        {
            expr_t *iid;

            if (is_conformance_needed_for_phase(phase) && (iid = get_attrp( var->attrs, ATTR_IIDIS )))
            {
                print_file( file, indent, "__frame->_StubMsg.MaxCount = (ULONG_PTR) " );
                write_expr( file, iid, 1, 1, NULL, NULL, local_var_prefix );
                fprintf( file, ";\n\n" );
            }
            break;
        }
        else if (is_ptr(type))
            type = type_pointer_get_ref(type);
        else
            break;
    }
}

static void write_remoting_arg(FILE *file, int indent, const var_t *func, const char *local_var_prefix,
                               enum pass pass, enum remoting_phase phase, const var_t *var)
{
    int in_attr, out_attr, pointer_type;
    const type_t *type = var->type;
    unsigned char rtype;
    size_t start_offset = type->typestring_offset;

    pointer_type = get_attrv(var->attrs, ATTR_POINTERTYPE);
    if (!pointer_type)
        pointer_type = RPC_FC_RP;

    in_attr = is_attr(var->attrs, ATTR_IN);
    out_attr = is_attr(var->attrs, ATTR_OUT);
    if (!in_attr && !out_attr)
        in_attr = 1;

    if (phase != PHASE_FREE)
        switch (pass)
        {
        case PASS_IN:
            if (!in_attr) return;
            break;
        case PASS_OUT:
            if (!out_attr) return;
            break;
        case PASS_RETURN:
            break;
        }

    write_parameter_conf_or_var_exprs(file, indent, local_var_prefix, phase, var);
    rtype = get_struct_type(type);

    if (is_context_handle(type))
    {
        if (phase == PHASE_MARSHAL)
        {
            if (pass == PASS_IN)
            {
                /* if the context_handle attribute appears in the chain of types
                 * without pointers being followed, then the context handle must
                 * be direct, otherwise it is a pointer */
                int is_ch_ptr = is_aliaschain_attr(type, ATTR_CONTEXTHANDLE) ? FALSE : TRUE;
                print_file(file, indent, "NdrClientContextMarshall(\n");
                print_file(file, indent + 1, "&__frame->_StubMsg,\n");
                print_file(file, indent + 1, "(NDR_CCONTEXT)%s%s%s,\n", is_ch_ptr ? "*" : "", local_var_prefix, var->name);
                print_file(file, indent + 1, "%s);\n", in_attr && out_attr ? "1" : "0");
            }
            else
            {
                print_file(file, indent, "NdrServerContextNewMarshall(\n");
                print_file(file, indent + 1, "&__frame->_StubMsg,\n");
                print_file(file, indent + 1, "(NDR_SCONTEXT)%s%s,\n", local_var_prefix, var->name);
                print_file(file, indent + 1, "(NDR_RUNDOWN)%s_rundown,\n", get_context_handle_type_name(var->type));
                print_file(file, indent + 1, "(PFORMAT_STRING)&__MIDL_TypeFormatString.Format[%d]);\n", start_offset);
            }
        }
        else if (phase == PHASE_UNMARSHAL)
        {
            if (pass == PASS_OUT)
            {
                if (!in_attr)
                    print_file(file, indent, "*%s%s = 0;\n", local_var_prefix, var->name);
                print_file(file, indent, "NdrClientContextUnmarshall(\n");
                print_file(file, indent + 1, "&__frame->_StubMsg,\n");
                print_file(file, indent + 1, "(NDR_CCONTEXT *)%s%s,\n", local_var_prefix, var->name);
                print_file(file, indent + 1, "__frame->_Handle);\n");
            }
            else
            {
                print_file(file, indent, "%s%s = NdrServerContextNewUnmarshall(\n", local_var_prefix, var->name);
                print_file(file, indent + 1, "&__frame->_StubMsg,\n");
                print_file(file, indent + 1, "(PFORMAT_STRING)&__MIDL_TypeFormatString.Format[%d]);\n", start_offset);
            }
        }
    }
    else if (is_user_type(var->type))
    {
        print_phase_function(file, indent, "UserMarshal", local_var_prefix, phase, var, start_offset);
    }
    else if (is_string_type(var->attrs, var->type))
    {
        if (is_array(type) && !is_conformant_array(type))
            print_phase_function(file, indent, "NonConformantString", local_var_prefix,
                                 phase, var, start_offset);
        else
        {
            if (phase == PHASE_FREE || pass == PASS_RETURN || pointer_type == RPC_FC_UP)
                print_phase_function(file, indent, "Pointer", local_var_prefix, phase, var,
                                     start_offset - (is_conformant_array(type) ? 4 : 2));
            else
                print_phase_function(file, indent, "ConformantString", local_var_prefix,
                                     phase, var, start_offset);
        }
    }
    else if (is_array(type))
    {
        unsigned char tc = get_array_type( type );
        const char *array_type = "FixedArray";

        /* We already have the size_is expression since it's at the
           top level, but do checks for multidimensional conformant
           arrays.  When we handle them, we'll need to extend this
           function to return a list, and then we'll actually use
           the return value.  */
        get_size_is_expr(type, var->name);

        if (tc == RPC_FC_SMVARRAY || tc == RPC_FC_LGVARRAY)
        {
            array_type = "VaryingArray";
        }
        else if (tc == RPC_FC_CARRAY)
        {
            array_type = "ConformantArray";
        }
        else if (tc == RPC_FC_CVARRAY || tc == RPC_FC_BOGUS_ARRAY)
        {
            array_type = (tc == RPC_FC_BOGUS_ARRAY
                          ? "ComplexArray"
                          : "ConformantVaryingArray");
        }

        if (pointer_type != RPC_FC_RP) array_type = "Pointer";
        print_phase_function(file, indent, array_type, local_var_prefix, phase, var, start_offset);
        if (phase == PHASE_FREE && pointer_type == RPC_FC_RP)
        {
            /* these are all unmarshalled by allocating memory */
            if (tc == RPC_FC_BOGUS_ARRAY ||
                tc == RPC_FC_CVARRAY ||
                ((tc == RPC_FC_SMVARRAY || tc == RPC_FC_LGVARRAY) && in_attr) ||
                (tc == RPC_FC_CARRAY && !in_attr))
            {
                print_file(file, indent, "if (%s%s)\n", local_var_prefix, var->name);
                indent++;
                print_file(file, indent, "__frame->_StubMsg.pfnFree(%s%s);\n", local_var_prefix, var->name);
            }
        }
    }
    else if (!is_ptr(var->type) && is_base_type(rtype))
    {
        if (phase != PHASE_FREE)
            print_phase_basetype(file, indent, local_var_prefix, phase, pass, var, var->name);
    }
    else if (!is_ptr(var->type))
    {
        switch (rtype)
        {
        case RPC_FC_STRUCT:
        case RPC_FC_PSTRUCT:
            print_phase_function(file, indent, "SimpleStruct", local_var_prefix, phase, var, start_offset);
            break;
        case RPC_FC_CSTRUCT:
        case RPC_FC_CPSTRUCT:
            print_phase_function(file, indent, "ConformantStruct", local_var_prefix, phase, var, start_offset);
            break;
        case RPC_FC_CVSTRUCT:
            print_phase_function(file, indent, "ConformantVaryingStruct", local_var_prefix, phase, var, start_offset);
            break;
        case RPC_FC_BOGUS_STRUCT:
            print_phase_function(file, indent, "ComplexStruct", local_var_prefix, phase, var, start_offset);
            break;
        default:
            error("write_remoting_arguments: Unsupported type: %s (0x%02x)\n", var->name, rtype);
        }
    }
    else
    {
        const type_t *ref = type_pointer_get_ref(type);
        if (type->type == RPC_FC_RP && is_base_type(ref->type))
        {
            if (phase != PHASE_FREE)
                print_phase_basetype(file, indent, local_var_prefix, phase, pass, var, var->name);
        }
        else if (type->type == RPC_FC_RP && get_struct_type(ref) == RPC_FC_STRUCT &&
                 !is_user_type(ref))
        {
            if (phase != PHASE_BUFFERSIZE && phase != PHASE_FREE)
                print_phase_function(file, indent, "SimpleStruct",
                                     local_var_prefix, phase, var,
                                     ref->typestring_offset);
        }
        else
        {
            if (ref->type == RPC_FC_IP)
                print_phase_function(file, indent, "InterfacePointer", local_var_prefix, phase, var, start_offset);
            else
                print_phase_function(file, indent, "Pointer", local_var_prefix, phase, var, start_offset);
        }
    }
    fprintf(file, "\n");
}

void write_remoting_arguments(FILE *file, int indent, const var_t *func, const char *local_var_prefix,
                              enum pass pass, enum remoting_phase phase)
{
    if (phase == PHASE_BUFFERSIZE && pass != PASS_RETURN)
    {
        unsigned int size = get_function_buffer_size( func, pass );
        print_file(file, indent, "__frame->_StubMsg.BufferLength = %u;\n", size);
    }

    if (pass == PASS_RETURN)
    {
        var_t var;
        var = *func;
        var.type = type_function_get_rettype(func->type);
        var.name = xstrdup( "_RetVal" );
        write_remoting_arg( file, indent, func, local_var_prefix, pass, phase, &var );
        free( var.name );
    }
    else
    {
        const var_t *var;
        if (!type_get_function_args(func->type))
            return;
        LIST_FOR_EACH_ENTRY( var, type_get_function_args(func->type), const var_t, entry )
            write_remoting_arg( file, indent, func, local_var_prefix, pass, phase, var );
    }
}


size_t get_size_procformatstring_type(const char *name, const type_t *type, const attr_list_t *attrs)
{
    return write_procformatstring_type(NULL, 0, name, type, attrs, FALSE);
}


size_t get_size_procformatstring_func(const var_t *func)
{
    const var_t *var;
    size_t size = 0;

    /* argument list size */
    if (type_get_function_args(func->type))
        LIST_FOR_EACH_ENTRY( var, type_get_function_args(func->type), const var_t, entry )
            size += get_size_procformatstring_type(var->name, var->type, var->attrs);

    /* return value size */
    if (is_void(type_function_get_rettype(func->type)))
        size += 2; /* FC_END and FC_PAD */
    else
        size += get_size_procformatstring_type("return value", type_function_get_rettype(func->type), NULL);

    return size;
}

size_t get_size_procformatstring(const statement_list_t *stmts, type_pred_t pred)
{
    const statement_t *stmt;
    size_t size = 1;

    if (stmts) LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
    {
        const type_t *iface;
        const statement_t *stmt_func;

        if (stmt->type == STMT_LIBRARY)
        {
            size += get_size_procformatstring(stmt->u.lib->stmts, pred) - 1;
            continue;
        }
        else if (stmt->type != STMT_TYPE || stmt->u.type->type != RPC_FC_IP)
            continue;

        iface = stmt->u.type;
        if (!pred(iface))
            continue;

        STATEMENTS_FOR_EACH_FUNC( stmt_func, type_iface_get_stmts(iface) )
        {
            const var_t *func = stmt_func->u.var;
            if (!is_local(func->attrs))
                size += get_size_procformatstring_func( func );
        }
    }
    return size;
}

size_t get_size_typeformatstring(const statement_list_t *stmts, type_pred_t pred)
{
    set_all_tfswrite(FALSE);
    return process_tfs(NULL, stmts, pred);
}

void declare_stub_args( FILE *file, int indent, const var_t *func )
{
    int in_attr, out_attr;
    int i = 0;
    const var_t *var;

    /* declare return value '_RetVal' */
    if (!is_void(type_function_get_rettype(func->type)))
    {
        print_file(file, indent, "");
        write_type_decl_left(file, type_function_get_rettype(func->type));
        fprintf(file, " _RetVal;\n");
    }

    if (!type_get_function_args(func->type))
        return;

    LIST_FOR_EACH_ENTRY( var, type_get_function_args(func->type), const var_t, entry )
    {
        int is_string = is_string_type(var->attrs, var->type);

        in_attr = is_attr(var->attrs, ATTR_IN);
        out_attr = is_attr(var->attrs, ATTR_OUT);
        if (!out_attr && !in_attr)
            in_attr = 1;

        if (is_context_handle(var->type))
            print_file(file, indent, "NDR_SCONTEXT %s;\n", var->name);
        else
        {
            if (!in_attr && !is_conformant_array(var->type) && !is_string)
            {
                type_t *type_to_print;
                print_file(file, indent, "");
                if (var->type->declarray)
                    type_to_print = var->type;
                else
                    type_to_print = type_pointer_get_ref(var->type);
                write_type_decl(file, type_to_print, "_W%u", i++);
                fprintf(file, ";\n");
            }

            print_file(file, indent, "");
            write_type_decl_left(file, var->type);
            fprintf(file, " ");
            if (var->type->declarray) {
                fprintf(file, "(*%s)", var->name);
            } else
                fprintf(file, "%s", var->name);
            write_type_right(file, var->type, FALSE);
            fprintf(file, ";\n");

            if (decl_indirect(var->type))
                print_file(file, indent, "void *_p_%s;\n", var->name);
        }
    }
}


void assign_stub_out_args( FILE *file, int indent, const var_t *func, const char *local_var_prefix )
{
    int in_attr, out_attr;
    int i = 0, sep = 0;
    const var_t *var;

    if (!type_get_function_args(func->type))
        return;

    LIST_FOR_EACH_ENTRY( var, type_get_function_args(func->type), const var_t, entry )
    {
        int is_string = is_string_type(var->attrs, var->type);
        in_attr = is_attr(var->attrs, ATTR_IN);
        out_attr = is_attr(var->attrs, ATTR_OUT);
        if (!out_attr && !in_attr)
            in_attr = 1;

        if (!in_attr)
        {
            print_file(file, indent, "%s%s", local_var_prefix, var->name);

            if (is_context_handle(var->type))
            {
                fprintf(file, " = NdrContextHandleInitialize(\n");
                print_file(file, indent + 1, "&__frame->_StubMsg,\n");
                print_file(file, indent + 1, "(PFORMAT_STRING)&__MIDL_TypeFormatString.Format[%d]);\n",
                           var->type->typestring_offset);
            }
            else if (is_array(var->type) &&
                     type_array_has_conformance(var->type))
            {
                unsigned int size, align = 0;
                type_t *type = var->type;

                fprintf(file, " = NdrAllocate(&__frame->_StubMsg, ");
                for ( ;
                     is_array(type) && type_array_has_conformance(type);
                     type = type_array_get_element(type))
                {
                    write_expr(file, type_array_get_conformance(type), TRUE,
                               TRUE, NULL, NULL, local_var_prefix);
                    fprintf(file, " * ");
                }
                size = type_memsize(type, &align);
                fprintf(file, "%u);\n", size);
            }
            else if (!is_string)
            {
                fprintf(file, " = &%s_W%u;\n", local_var_prefix, i);
                if (is_ptr(var->type) && !last_ptr(var->type))
                    print_file(file, indent, "%s_W%u = 0;\n", local_var_prefix, i);
                i++;
            }

            sep = 1;
        }
    }
    if (sep)
        fprintf(file, "\n");
}


int write_expr_eval_routines(FILE *file, const char *iface)
{
    static const char *var_name = "pS";
    static const char *var_name_expr = "pS->";
    int result = 0;
    struct expr_eval_routine *eval;
    unsigned short callback_offset = 0;

    LIST_FOR_EACH_ENTRY(eval, &expr_eval_routines, struct expr_eval_routine, entry)
    {
        const char *name = eval->structure->name;
        result = 1;

        print_file(file, 0, "static void __RPC_USER %s_%sExprEval_%04u(PMIDL_STUB_MESSAGE pStubMsg)\n",
                   iface, name, callback_offset);
        print_file(file, 0, "{\n");
        print_file (file, 1, "%s *%s = (%s *)(pStubMsg->StackTop - %u);\n",
                    name, var_name, name, eval->baseoff);
        print_file(file, 1, "pStubMsg->Offset = 0;\n"); /* FIXME */
        print_file(file, 1, "pStubMsg->MaxCount = (ULONG_PTR)");
        write_expr(file, eval->expr, 1, 1, var_name_expr, eval->structure, "");
        fprintf(file, ";\n");
        print_file(file, 0, "}\n\n");
        callback_offset++;
    }
    return result;
}

void write_expr_eval_routine_list(FILE *file, const char *iface)
{
    struct expr_eval_routine *eval;
    struct expr_eval_routine *cursor;
    unsigned short callback_offset = 0;

    fprintf(file, "static const EXPR_EVAL ExprEvalRoutines[] =\n");
    fprintf(file, "{\n");

    LIST_FOR_EACH_ENTRY_SAFE(eval, cursor, &expr_eval_routines, struct expr_eval_routine, entry)
    {
        const char *name = eval->structure->name;
        print_file(file, 1, "%s_%sExprEval_%04u,\n", iface, name, callback_offset);
        callback_offset++;
        list_remove(&eval->entry);
        free(eval);
    }

    fprintf(file, "};\n\n");
}

void write_user_quad_list(FILE *file)
{
    user_type_t *ut;

    if (list_empty(&user_type_list))
        return;

    fprintf(file, "static const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[] =\n");
    fprintf(file, "{\n");
    LIST_FOR_EACH_ENTRY(ut, &user_type_list, user_type_t, entry)
    {
        const char *sep = &ut->entry == list_tail(&user_type_list) ? "" : ",";
        print_file(file, 1, "{\n");
        print_file(file, 2, "(USER_MARSHAL_SIZING_ROUTINE)%s_UserSize,\n", ut->name);
        print_file(file, 2, "(USER_MARSHAL_MARSHALLING_ROUTINE)%s_UserMarshal,\n", ut->name);
        print_file(file, 2, "(USER_MARSHAL_UNMARSHALLING_ROUTINE)%s_UserUnmarshal,\n", ut->name);
        print_file(file, 2, "(USER_MARSHAL_FREEING_ROUTINE)%s_UserFree\n", ut->name);
        print_file(file, 1, "}%s\n", sep);
    }
    fprintf(file, "};\n\n");
}

void write_endpoints( FILE *f, const char *prefix, const str_list_t *list )
{
    const struct str_list_entry_t *endpoint;
    const char *p;

    /* this should be an array of RPC_PROTSEQ_ENDPOINT but we want const strings */
    print_file( f, 0, "static const unsigned char * const %s__RpcProtseqEndpoint[][2] =\n{\n", prefix );
    LIST_FOR_EACH_ENTRY( endpoint, list, const struct str_list_entry_t, entry )
    {
        print_file( f, 1, "{ (const unsigned char *)\"" );
        for (p = endpoint->str; *p && *p != ':'; p++)
        {
            if (*p == '"' || *p == '\\') fputc( '\\', f );
            fputc( *p, f );
        }
        if (!*p) goto error;
        if (p[1] != '[') goto error;

        fprintf( f, "\", (const unsigned char *)\"" );
        for (p += 2; *p && *p != ']'; p++)
        {
            if (*p == '"' || *p == '\\') fputc( '\\', f );
            fputc( *p, f );
        }
        if (*p != ']') goto error;
        fprintf( f, "\" },\n" );
    }
    print_file( f, 0, "};\n\n" );
    return;

error:
    error("Invalid endpoint syntax '%s'\n", endpoint->str);
}

void write_exceptions( FILE *file )
{
    fprintf( file, "#ifndef USE_COMPILER_EXCEPTIONS\n");
    fprintf( file, "\n");
    fprintf( file, "#include \"wine/exception.h\"\n");
    fprintf( file, "#undef RpcTryExcept\n");
    fprintf( file, "#undef RpcExcept\n");
    fprintf( file, "#undef RpcEndExcept\n");
    fprintf( file, "#undef RpcTryFinally\n");
    fprintf( file, "#undef RpcFinally\n");
    fprintf( file, "#undef RpcEndFinally\n");
    fprintf( file, "#undef RpcExceptionCode\n");
    fprintf( file, "#undef RpcAbnormalTermination\n");
    fprintf( file, "\n");
    fprintf( file, "struct __exception_frame;\n");
    fprintf( file, "typedef int (*__filter_func)(EXCEPTION_RECORD *, struct __exception_frame *);\n");
    fprintf( file, "typedef void (*__finally_func)(struct __exception_frame *);\n");
    fprintf( file, "\n");
    fprintf( file, "#define __DECL_EXCEPTION_FRAME \\\n");
    fprintf( file, "    EXCEPTION_REGISTRATION_RECORD frame; \\\n");
    fprintf( file, "    __filter_func                 filter; \\\n");
    fprintf( file, "    __finally_func                finally; \\\n");
    fprintf( file, "    sigjmp_buf                    jmp; \\\n");
    fprintf( file, "    DWORD                         code; \\\n");
    fprintf( file, "    unsigned char                 abnormal_termination; \\\n");
    fprintf( file, "    unsigned char                 filter_level; \\\n");
    fprintf( file, "    unsigned char                 finally_level;\n");
    fprintf( file, "\n");
    fprintf( file, "struct __exception_frame\n{\n");
    fprintf( file, "    __DECL_EXCEPTION_FRAME\n");
    fprintf( file, "};\n");
    fprintf( file, "\n");
    fprintf( file, "static DWORD __widl_exception_handler( EXCEPTION_RECORD *record,\n");
    fprintf( file, "                                       EXCEPTION_REGISTRATION_RECORD *frame,\n");
    fprintf( file, "                                       CONTEXT *context,\n");
    fprintf( file, "                                       EXCEPTION_REGISTRATION_RECORD **pdispatcher )\n");
    fprintf( file, "{\n");
    fprintf( file, "    struct __exception_frame *exc_frame = (struct __exception_frame *)frame;\n");
    fprintf( file, "\n");
    fprintf( file, "    if (record->ExceptionFlags & (EH_UNWINDING | EH_EXIT_UNWIND | EH_NESTED_CALL))\n");
    fprintf( file, "    {\n" );
    fprintf( file, "        if (exc_frame->finally_level && (record->ExceptionFlags & (EH_UNWINDING | EH_EXIT_UNWIND)))\n");
    fprintf( file, "        {\n" );
    fprintf( file, "            exc_frame->abnormal_termination = 1;\n");
    fprintf( file, "            exc_frame->finally( exc_frame );\n");
    fprintf( file, "        }\n" );
    fprintf( file, "        return ExceptionContinueSearch;\n");
    fprintf( file, "    }\n" );
    fprintf( file, "    exc_frame->code = record->ExceptionCode;\n");
    fprintf( file, "    if (exc_frame->filter_level && exc_frame->filter( record, exc_frame ) == EXCEPTION_EXECUTE_HANDLER)\n" );
    fprintf( file, "    {\n");
    fprintf( file, "        __wine_rtl_unwind( frame, record );\n");
    fprintf( file, "        if (exc_frame->finally_level > exc_frame->filter_level)\n" );
    fprintf( file, "        {\n");
    fprintf( file, "            exc_frame->abnormal_termination = 1;\n");
    fprintf( file, "            exc_frame->finally( exc_frame );\n");
    fprintf( file, "            __wine_pop_frame( frame );\n");
    fprintf( file, "        }\n");
    fprintf( file, "        exc_frame->filter_level = 0;\n");
    fprintf( file, "        siglongjmp( exc_frame->jmp, 1 );\n");
    fprintf( file, "    }\n");
    fprintf( file, "    return ExceptionContinueSearch;\n");
    fprintf( file, "}\n");
    fprintf( file, "\n");
    fprintf( file, "#define RpcTryExcept \\\n");
    fprintf( file, "    if (!sigsetjmp( __frame->jmp, 0 )) \\\n");
    fprintf( file, "    { \\\n");
    fprintf( file, "        if (!__frame->finally_level) \\\n" );
    fprintf( file, "            __wine_push_frame( &__frame->frame ); \\\n");
    fprintf( file, "        __frame->filter_level = __frame->finally_level + 1;\n" );
    fprintf( file, "\n");
    fprintf( file, "#define RpcExcept(expr) \\\n");
    fprintf( file, "        if (!__frame->finally_level) \\\n" );
    fprintf( file, "            __wine_pop_frame( &__frame->frame ); \\\n");
    fprintf( file, "        __frame->filter_level = 0; \\\n" );
    fprintf( file, "    } \\\n");
    fprintf( file, "    else \\\n");
    fprintf( file, "\n");
    fprintf( file, "#define RpcEndExcept\n");
    fprintf( file, "\n");
    fprintf( file, "#define RpcExceptionCode() (__frame->code)\n");
    fprintf( file, "\n");
    fprintf( file, "#define RpcTryFinally \\\n");
    fprintf( file, "    if (!__frame->filter_level) \\\n");
    fprintf( file, "        __wine_push_frame( &__frame->frame ); \\\n");
    fprintf( file, "    __frame->finally_level = __frame->filter_level + 1;\n");
    fprintf( file, "\n");
    fprintf( file, "#define RpcFinally \\\n");
    fprintf( file, "    if (!__frame->filter_level) \\\n");
    fprintf( file, "        __wine_pop_frame( &__frame->frame ); \\\n");
    fprintf( file, "    __frame->finally_level = 0;\n");
    fprintf( file, "\n");
    fprintf( file, "#define RpcEndFinally\n");
    fprintf( file, "\n");
    fprintf( file, "#define RpcAbnormalTermination() (__frame->abnormal_termination)\n");
    fprintf( file, "\n");
    fprintf( file, "#define RpcExceptionInit(filter_func,finally_func) \\\n");
    fprintf( file, "    do { \\\n");
    fprintf( file, "        __frame->frame.Handler = __widl_exception_handler; \\\n");
    fprintf( file, "        __frame->filter = (__filter_func)(filter_func); \\\n" );
    fprintf( file, "        __frame->finally = (__finally_func)(finally_func); \\\n");
    fprintf( file, "        __frame->abnormal_termination = 0; \\\n");
    fprintf( file, "        __frame->filter_level = 0; \\\n");
    fprintf( file, "        __frame->finally_level = 0; \\\n");
    fprintf( file, "    } while (0)\n");
    fprintf( file, "\n");
    fprintf( file, "#else /* USE_COMPILER_EXCEPTIONS */\n");
    fprintf( file, "\n");
    fprintf( file, "#define RpcExceptionInit(filter_func,finally_func) do {} while(0)\n");
    fprintf( file, "#define __DECL_EXCEPTION_FRAME\n");
    fprintf( file, "\n");
    fprintf( file, "#endif /* USE_COMPILER_EXCEPTIONS */\n");
}
