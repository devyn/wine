/*
 * IDL Type Tree
 *
 * Copyright 2008 Robert Shearman
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

#include "widltypes.h"
#include <assert.h>

#ifndef WIDL_TYPE_TREE_H
#define WIDL_TYPE_TREE_H

type_t *type_new_function(var_list_t *args);
type_t *type_new_pointer(type_t *ref, attr_list_t *attrs);
type_t *type_new_alias(type_t *t, const char *name);
type_t *type_new_module(char *name);
type_t *type_new_array(const char *name, type_t *element, int declarray,
                       unsigned long dim, expr_t *size_is, expr_t *length_is);
void type_interface_define(type_t *iface, type_t *inherit, statement_list_t *stmts);
void type_dispinterface_define(type_t *iface, var_list_t *props, func_list_t *methods);
void type_dispinterface_define_from_iface(type_t *dispiface, type_t *iface);
void type_module_define(type_t *module, statement_list_t *stmts);
type_t *type_coclass_define(type_t *coclass, ifref_list_t *ifaces);

/* FIXME: shouldn't need to export this */
type_t *duptype(type_t *t, int dupname);

static inline var_list_t *type_struct_get_fields(const type_t *type)
{
    assert(is_struct(type->type));
    return type->details.structure->fields;
}

static inline var_list_t *type_function_get_args(const type_t *type)
{
    assert(type->type == RPC_FC_FUNCTION);
    return type->details.function->args;
}

static inline type_t *type_function_get_rettype(const type_t *type)
{
    assert(type->type == RPC_FC_FUNCTION);
    return type->ref;
}

static inline var_list_t *type_enum_get_values(const type_t *type)
{
    assert(type->type == RPC_FC_ENUM16 || type->type == RPC_FC_ENUM32);
    return type->details.enumeration->enums;
}

static inline var_t *type_union_get_switch_value(const type_t *type)
{
    assert(type->type == RPC_FC_ENCAPSULATED_UNION);
    return LIST_ENTRY(list_head(type->details.structure->fields), var_t, entry);
}

static inline var_list_t *type_encapsulated_union_get_fields(const type_t *type)
{
    assert(type->type == RPC_FC_ENCAPSULATED_UNION);
    return type->details.structure->fields;
}

static inline var_list_t *type_union_get_cases(const type_t *type)
{
    assert(type->type == RPC_FC_ENCAPSULATED_UNION ||
           type->type == RPC_FC_NON_ENCAPSULATED_UNION);
    if (type->type == RPC_FC_ENCAPSULATED_UNION)
    {
        const var_t *uv = LIST_ENTRY(list_tail(type->details.structure->fields), const var_t, entry);
        return uv->type->details.structure->fields;
    }
    else
        return type->details.structure->fields;
}

static inline statement_list_t *type_iface_get_stmts(const type_t *type)
{
    assert(type->type == RPC_FC_IP);
    return type->details.iface->stmts;
}

static inline type_t *type_iface_get_inherit(const type_t *type)
{
    assert(type->type == RPC_FC_IP);
    return type->ref;
}

static inline var_list_t *type_dispiface_get_props(const type_t *type)
{
    assert(type->type == RPC_FC_IP);
    return type->details.iface->disp_props;
}

static inline var_list_t *type_dispiface_get_methods(const type_t *type)
{
    assert(type->type == RPC_FC_IP);
    return type->details.iface->disp_methods;
}

static inline int type_is_defined(const type_t *type)
{
    return type->defined;
}

static inline int type_is_complete(const type_t *type)
{
    if (type->type == RPC_FC_FUNCTION)
        return (type->details.function != NULL);
    else if (type->type == RPC_FC_IP)
        return (type->details.iface != NULL);
    else if (type->type == RPC_FC_ENUM16 || type->type == RPC_FC_ENUM32)
        return (type->details.enumeration != NULL);
    else if (is_struct(type->type) || is_union(type->type))
        return (type->details.structure != NULL);
    else
        return TRUE;
}

static inline int type_array_has_conformance(const type_t *type)
{
    assert(is_array(type));
    return (type->details.array.size_is != NULL);
}

static inline int type_array_has_variance(const type_t *type)
{
    assert(is_array(type));
    return (type->details.array.length_is != NULL);
}

static inline unsigned long type_array_get_dim(const type_t *type)
{
    assert(is_array(type));
    return type->details.array.dim;
}

static inline expr_t *type_array_get_conformance(const type_t *type)
{
    assert(is_array(type));
    return type->details.array.size_is;
}

static inline expr_t *type_array_get_variance(const type_t *type)
{
    assert(is_array(type));
    return type->details.array.length_is;
}

static inline type_t *type_array_get_element(const type_t *type)
{
    assert(is_array(type));
    return type->ref;
}

static inline type_t *type_get_real_type(const type_t *type)
{
    if (type->is_alias)
        return type_get_real_type(type->orig);
    else
        return (type_t *)type;
}

static inline int type_is_alias(const type_t *type)
{
    return type->is_alias;
}

static inline ifref_list_t *type_coclass_get_ifaces(const type_t *type)
{
    assert(type->type == RPC_FC_COCLASS);
    return type->details.coclass.ifaces;
}

static inline type_t *type_pointer_get_ref(const type_t *type)
{
    assert(is_ptr(type));
    return type->ref;
}

#endif /* WIDL_TYPE_TREE_H */
