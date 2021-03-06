/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * D-Bus Simulator
 * Copyright (C) Philip Withnall 2011 <philip@tecnocode.co.uk>
 * 
 * D-Bus Simulator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * D-Bus Simulator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with D-Bus Simulator.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:dfsm-ast-data-structure
 * @short_description: AST data structure node
 * @stability: Unstable
 * @include: dfsm/dfsm-ast-data-structure.h
 *
 * AST node representing any instance of a basic or compound data structure. This supports (recursive) fuzzing of data structure values.
 */

#include "config.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include "dfsm-ast-data-structure.h"
#include "dfsm-ast-expression-data-structure.h"
#include "dfsm-ast-variable.h"
#include "dfsm-parser.h"
#include "dfsm-parser-internal.h"
#include "dfsm-probabilities.h"

static void dfsm_ast_data_structure_finalize (GObject *object);
static void dfsm_ast_data_structure_sanity_check (DfsmAstNode *node);
static void dfsm_ast_data_structure_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static void dfsm_ast_data_structure_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);

struct _DfsmAstDataStructurePrivate {
	DfsmAstDataStructureType data_structure_type;
	GVariantType *variant_type; /* gets set by _calculate_type(); NULL beforehand; always a definite type when set */
	gdouble weight;
	gchar *type_annotation; /* may not match the actual data in the structure until after dfsm_ast_data_structure_check() is called */
	gchar *nickname; /* may be NULL; must not be the empty string */
	gchar *unparsed_string; /* may be NULL; used by integer/double values before being parsed in pre_check_and_register() */
	union {
		guchar byte_val;
		gboolean boolean_val;
		gint16 int16_val;
		guint16 uint16_val;
		gint32 int32_val;
		guint32 uint32_val;
		gint64 int64_val;
		guint64 uint64_val;
		gdouble double_val;
		gchar *string_val;
		gchar *object_path_val;
		gchar *signature_val;
		GPtrArray/*<DfsmAstExpression>*/ *array_val;
		GPtrArray/*<DfsmAstExpression>*/ *struct_val;
		DfsmAstExpression *variant_val;
		GPtrArray/*<DfsmAstDictionaryEntry>*/ *dict_val;
		gint unix_fd_val;
		DfsmAstVariable *variable_val;
	};
};

G_DEFINE_TYPE (DfsmAstDataStructure, dfsm_ast_data_structure, DFSM_TYPE_AST_NODE)

static void
dfsm_ast_data_structure_class_init (DfsmAstDataStructureClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	DfsmAstNodeClass *node_class = DFSM_AST_NODE_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmAstDataStructurePrivate));

	gobject_class->finalize = dfsm_ast_data_structure_finalize;

	node_class->sanity_check = dfsm_ast_data_structure_sanity_check;
	node_class->pre_check_and_register = dfsm_ast_data_structure_pre_check_and_register;
	node_class->check = dfsm_ast_data_structure_check;
}

static void
dfsm_ast_data_structure_init (DfsmAstDataStructure *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DFSM_TYPE_AST_DATA_STRUCTURE, DfsmAstDataStructurePrivate);
}

static void
dfsm_ast_data_structure_finalize (GObject *object)
{
	DfsmAstDataStructurePrivate *priv = DFSM_AST_DATA_STRUCTURE (object)->priv;

	if (priv->variant_type != NULL) {
		g_variant_type_free (priv->variant_type);
	}

	g_free (priv->type_annotation);
	g_free (priv->nickname);
	g_free (priv->unparsed_string);

	switch (priv->data_structure_type) {
		case DFSM_AST_DATA_BYTE:
		case DFSM_AST_DATA_BOOLEAN:
		case DFSM_AST_DATA_INT16:
		case DFSM_AST_DATA_UINT16:
		case DFSM_AST_DATA_INT32:
		case DFSM_AST_DATA_UINT32:
		case DFSM_AST_DATA_INT64:
		case DFSM_AST_DATA_UINT64:
		case DFSM_AST_DATA_DOUBLE:
			/* Nothing to free here */
			break;
		case DFSM_AST_DATA_STRING:
			g_free (priv->string_val);
			break;
		case DFSM_AST_DATA_OBJECT_PATH:
			g_free (priv->object_path_val);
			break;
		case DFSM_AST_DATA_SIGNATURE:
			g_free (priv->signature_val);
			break;
		case DFSM_AST_DATA_ARRAY:
			g_ptr_array_unref (priv->array_val);
			break;
		case DFSM_AST_DATA_STRUCT:
			g_ptr_array_unref (priv->struct_val);
			break;
		case DFSM_AST_DATA_VARIANT:
			g_object_unref (priv->variant_val);
			break;
		case DFSM_AST_DATA_DICT:
			g_ptr_array_unref (priv->dict_val);
			break;
		case DFSM_AST_DATA_UNIX_FD:
			/* Nothing to free here */
			break;
		case DFSM_AST_DATA_VARIABLE:
			g_object_unref (priv->variable_val);
			break;
		default:
			g_assert_not_reached ();
	}

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_data_structure_parent_class)->finalize (object);
}

static void
dfsm_ast_data_structure_sanity_check (DfsmAstNode *node)
{
	DfsmAstDataStructurePrivate *priv = DFSM_AST_DATA_STRUCTURE (node)->priv;
	guint i;

	g_assert (priv->type_annotation == NULL || *(priv->type_annotation) != '\0');
	g_assert (priv->nickname == NULL || *(priv->nickname) != '\0');

	switch (priv->data_structure_type) {
		case DFSM_AST_DATA_BYTE:
		case DFSM_AST_DATA_BOOLEAN:
		case DFSM_AST_DATA_INT16:
		case DFSM_AST_DATA_UINT16:
		case DFSM_AST_DATA_INT32:
		case DFSM_AST_DATA_UINT32:
		case DFSM_AST_DATA_INT64:
		case DFSM_AST_DATA_UINT64:
		case DFSM_AST_DATA_DOUBLE:
			/* Nothing to do here */
			break;
		case DFSM_AST_DATA_STRING:
			g_assert (priv->string_val != NULL);
			break;
		case DFSM_AST_DATA_OBJECT_PATH:
			g_assert (priv->object_path_val != NULL);
			break;
		case DFSM_AST_DATA_SIGNATURE:
			g_assert (priv->signature_val != NULL);
			break;
		case DFSM_AST_DATA_ARRAY:
			g_assert (priv->array_val != NULL);

			for (i = 0; i < priv->array_val->len; i++) {
				g_assert (g_ptr_array_index (priv->array_val, i) != NULL);
				dfsm_ast_node_sanity_check (DFSM_AST_NODE (g_ptr_array_index (priv->array_val, i)));
			}

			break;
		case DFSM_AST_DATA_STRUCT:
			g_assert (priv->struct_val != NULL);

			for (i = 0; i < priv->struct_val->len; i++) {
				g_assert (g_ptr_array_index (priv->struct_val, i) != NULL);
				dfsm_ast_node_sanity_check (DFSM_AST_NODE (g_ptr_array_index (priv->struct_val, i)));
			}

			break;
		case DFSM_AST_DATA_VARIANT:
			g_assert (DFSM_IS_AST_EXPRESSION (priv->variant_val));
			dfsm_ast_node_sanity_check (DFSM_AST_NODE (priv->variant_val));
			break;
		case DFSM_AST_DATA_DICT:
			g_assert (priv->dict_val != NULL);

			for (i = 0; i < priv->dict_val->len; i++) {
				DfsmAstDictionaryEntry *dict_val = (DfsmAstDictionaryEntry*) g_ptr_array_index (priv->dict_val, i);

				g_assert (dict_val != NULL);
				dfsm_ast_node_sanity_check (DFSM_AST_NODE (dict_val->key));
				dfsm_ast_node_sanity_check (DFSM_AST_NODE (dict_val->value));
			}

			break;
		case DFSM_AST_DATA_UNIX_FD:
			/* Nothing to do here */
			break;
		case DFSM_AST_DATA_VARIABLE:
			g_assert (priv->variable_val != NULL);
			dfsm_ast_node_sanity_check (DFSM_AST_NODE (priv->variable_val));
			break;
		default:
			g_assert_not_reached ();
	}
}

static guint64
parse_unsigned_integer (DfsmAstDataStructure *self, guint64 max_val, GError **error)
{
	guint64 parsed_val;

	/* Parse the string value and check for overflow. */
	errno = 0;
	parsed_val = g_ascii_strtoull (self->priv->unparsed_string, NULL, 10);

	if (errno == ERANGE || parsed_val > max_val) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Unsigned integer too wide: %s"), self->priv->unparsed_string);
		return 0;
	}

	/* Free the unparsed string. */
	g_free (self->priv->unparsed_string);
	self->priv->unparsed_string = NULL;

	return parsed_val;
}

static gint64
parse_signed_integer (DfsmAstDataStructure *self, gint64 min_val, gint64 max_val, GError **error)
{
	gint64 parsed_val;

	/* Parse the string value and check for overflow. */
	errno = 0;
	parsed_val = g_ascii_strtoll (self->priv->unparsed_string, NULL, 10);

	if (errno == ERANGE || parsed_val < min_val || parsed_val > max_val) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Signed integer too wide: %s"), self->priv->unparsed_string);
		return 0;
	}

	/* Free the unparsed string. */
	g_free (self->priv->unparsed_string);
	self->priv->unparsed_string = NULL;

	return parsed_val;
}

static gdouble
parse_double (DfsmAstDataStructure *self, GError **error)
{
	gdouble parsed_val;

	/* Parse the string value and check for overflow. */
	errno = 0;
	parsed_val = g_ascii_strtod (self->priv->unparsed_string, NULL);

	if (errno == ERANGE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Double too wide: %s"), self->priv->unparsed_string);
		return 0.0;
	}

	/* Free the unparsed string. */
	g_free (self->priv->unparsed_string);
	self->priv->unparsed_string = NULL;

	return parsed_val;
}

static gboolean
is_integer_type (DfsmAstDataStructureType data_structure_type)
{
	switch (data_structure_type) {
		case DFSM_AST_DATA_BYTE:
		case DFSM_AST_DATA_UINT16:
		case DFSM_AST_DATA_UINT32:
		case DFSM_AST_DATA_UINT64:
		case DFSM_AST_DATA_INT16:
		case DFSM_AST_DATA_INT32:
		case DFSM_AST_DATA_INT64:
			return TRUE;
		case DFSM_AST_DATA_BOOLEAN:
		case DFSM_AST_DATA_DOUBLE:
		case DFSM_AST_DATA_STRING:
		case DFSM_AST_DATA_OBJECT_PATH:
		case DFSM_AST_DATA_SIGNATURE:
		case DFSM_AST_DATA_ARRAY:
		case DFSM_AST_DATA_STRUCT:
		case DFSM_AST_DATA_VARIANT:
		case DFSM_AST_DATA_DICT:
		case DFSM_AST_DATA_UNIX_FD:
		case DFSM_AST_DATA_VARIABLE:
			return FALSE;
		default:
			g_assert_not_reached ();
	}
}

static void
dfsm_ast_data_structure_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstDataStructure *self = DFSM_AST_DATA_STRUCTURE (node);
	DfsmAstDataStructurePrivate *priv = self->priv;
	GVariantType *annotated_type;
	guint i;

	/* See if our type annotation is sane (if we have one), and set the data_structure_type from it if appropriate. Note that this only works
	 * when handling literal values, rather than variables, since we don't know the types of variables at this point. That's fine, though, because
	 * in the case of literal values, we're only using type annotations to infer their actual type. */
	if (priv->type_annotation != NULL) {
		if (g_variant_type_string_is_valid (priv->type_annotation) == FALSE) {
			g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Invalid type annotation: %s"), priv->type_annotation);
			return;
		}

		annotated_type = g_variant_type_new (priv->type_annotation);

		if (is_integer_type (priv->data_structure_type) == TRUE) {
			if (g_variant_type_equal (annotated_type, G_VARIANT_TYPE_BYTE) == TRUE) {
				priv->data_structure_type = DFSM_AST_DATA_BYTE;
			} else if (g_variant_type_equal (annotated_type, G_VARIANT_TYPE_UINT16) == TRUE) {
				priv->data_structure_type = DFSM_AST_DATA_UINT16;
			} else if (g_variant_type_equal (annotated_type, G_VARIANT_TYPE_UINT32) == TRUE) {
				priv->data_structure_type = DFSM_AST_DATA_UINT32;
			} else if (g_variant_type_equal (annotated_type, G_VARIANT_TYPE_UINT64) == TRUE) {
				priv->data_structure_type = DFSM_AST_DATA_UINT64;
			} else if (g_variant_type_equal (annotated_type, G_VARIANT_TYPE_INT16) == TRUE) {
				priv->data_structure_type = DFSM_AST_DATA_INT16;
			} else if (g_variant_type_equal (annotated_type, G_VARIANT_TYPE_INT32) == TRUE) {
				priv->data_structure_type = DFSM_AST_DATA_INT32;
			} else if (g_variant_type_equal (annotated_type, G_VARIANT_TYPE_INT64) == TRUE) {
				priv->data_structure_type = DFSM_AST_DATA_INT64;
			}
		} else if (g_variant_type_equal (annotated_type, G_VARIANT_TYPE_OBJECT_PATH) == TRUE &&
		           (priv->data_structure_type == DFSM_AST_DATA_OBJECT_PATH || priv->data_structure_type == DFSM_AST_DATA_STRING)) {
			priv->data_structure_type = DFSM_AST_DATA_OBJECT_PATH;
		} else if (g_variant_type_equal (annotated_type, G_VARIANT_TYPE_SIGNATURE) == TRUE &&
		           (priv->data_structure_type == DFSM_AST_DATA_SIGNATURE || priv->data_structure_type == DFSM_AST_DATA_STRING)) {
			priv->data_structure_type = DFSM_AST_DATA_SIGNATURE;
		}

		g_variant_type_free (annotated_type);
	}

	switch (priv->data_structure_type) {
		case DFSM_AST_DATA_BOOLEAN:
			/* Nothing to do here */
			break;
		case DFSM_AST_DATA_BYTE:
			priv->byte_val = parse_unsigned_integer (self, 255, error);
			break;
		case DFSM_AST_DATA_UINT16:
			priv->uint16_val = parse_unsigned_integer (self, G_MAXUINT16, error);
			break;
		case DFSM_AST_DATA_UINT32:
			priv->uint32_val = parse_unsigned_integer (self, G_MAXUINT32, error);
			break;
		case DFSM_AST_DATA_UINT64:
			priv->uint64_val = parse_unsigned_integer (self, G_MAXUINT64, error);
			break;
		case DFSM_AST_DATA_INT16:
			priv->int16_val = parse_signed_integer (self, G_MININT16, G_MAXINT16, error);
			break;
		case DFSM_AST_DATA_INT32:
			priv->int32_val = parse_signed_integer (self, G_MININT32, G_MAXINT32, error);
			break;
		case DFSM_AST_DATA_INT64:
			priv->int64_val = parse_signed_integer (self, G_MININT64, G_MAXINT64, error);
			break;
		case DFSM_AST_DATA_DOUBLE:
			priv->double_val = parse_double (self, error);
			break;
		case DFSM_AST_DATA_STRING:
			/* Valid UTF-8? */
			if (g_utf8_validate (priv->string_val, -1, NULL) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             _("Invalid UTF-8 in string: %s"), priv->string_val);
				return;
			}

			break;
		case DFSM_AST_DATA_OBJECT_PATH:
			/* Valid object path? */
			if (g_variant_is_object_path (priv->object_path_val) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             _("Invalid D-Bus object path: %s"), priv->object_path_val);
				return;
			}

			break;
		case DFSM_AST_DATA_SIGNATURE:
			/* Valid signature? */
			if (g_variant_type_string_is_valid ((gchar*) priv->signature_val) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             _("Invalid D-Bus type signature: %s"), priv->signature_val);
				return;
			}

			break;
		case DFSM_AST_DATA_ARRAY: {
			/* All entries valid? */
			for (i = 0; i < priv->array_val->len; i++) {
				DfsmAstExpression *expr;

				expr = g_ptr_array_index (priv->array_val, i);

				dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (expr), environment, error);

				if (*error != NULL) {
					return;
				}
			}

			break;
		}
		case DFSM_AST_DATA_STRUCT: {
			/* All entries valid? */
			for (i = 0; i < priv->struct_val->len; i++) {
				DfsmAstExpression *expr;

				expr = g_ptr_array_index (priv->struct_val, i);

				dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (expr), environment, error);

				if (*error != NULL) {
					return;
				}
			}

			break;
		}
		case DFSM_AST_DATA_VARIANT:
			dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (priv->variant_val), environment, error);

			if (*error != NULL) {
				return;
			}

			break;
		case DFSM_AST_DATA_DICT: {
			/* All entries valid? */
			for (i = 0; i < priv->dict_val->len; i++) {
				DfsmAstDictionaryEntry *entry;

				/* Valid expressions? */
				entry = g_ptr_array_index (priv->dict_val, i);

				dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (entry->key), environment, error);

				if (*error != NULL) {
					return;
				}

				dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (entry->value), environment, error);

				if (*error != NULL) {
					return;
				}
			}

			break;
		}
		case DFSM_AST_DATA_UNIX_FD:
			/* Nothing to do here */
			break;
		case DFSM_AST_DATA_VARIABLE:
			/* Valid variable? */
			dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (priv->variable_val), environment, error);

			if (*error != NULL) {
				return;
			}

			break;
		default:
			g_assert_not_reached ();
	}
}

static GVariantType *
__calculate_type (DfsmAstDataStructure *self, DfsmEnvironment *environment)
{
	DfsmAstDataStructurePrivate *priv = self->priv;

	switch (priv->data_structure_type) {
		case DFSM_AST_DATA_BYTE:
			return g_variant_type_copy (G_VARIANT_TYPE_BYTE);
		case DFSM_AST_DATA_BOOLEAN:
			return g_variant_type_copy (G_VARIANT_TYPE_BOOLEAN);
		case DFSM_AST_DATA_INT16:
			return g_variant_type_copy (G_VARIANT_TYPE_INT16);
		case DFSM_AST_DATA_UINT16:
			return g_variant_type_copy (G_VARIANT_TYPE_UINT16);
		case DFSM_AST_DATA_INT32:
			return g_variant_type_copy (G_VARIANT_TYPE_INT32);
		case DFSM_AST_DATA_UINT32:
			return g_variant_type_copy (G_VARIANT_TYPE_UINT32);
		case DFSM_AST_DATA_INT64:
			return g_variant_type_copy (G_VARIANT_TYPE_INT64);
		case DFSM_AST_DATA_UINT64:
			return g_variant_type_copy (G_VARIANT_TYPE_UINT64);
		case DFSM_AST_DATA_DOUBLE:
			return g_variant_type_copy (G_VARIANT_TYPE_DOUBLE);
		case DFSM_AST_DATA_STRING:
			return g_variant_type_copy (G_VARIANT_TYPE_STRING);
		case DFSM_AST_DATA_OBJECT_PATH:
			return g_variant_type_copy (G_VARIANT_TYPE_OBJECT_PATH);
		case DFSM_AST_DATA_SIGNATURE:
			return g_variant_type_copy (G_VARIANT_TYPE_SIGNATURE);
		case DFSM_AST_DATA_ARRAY: {
			GVariantType *array_type, *lg_child_type = NULL;
			guint i;

			/* We need to find a least general supertype of all the types in the array. */
			for (i = 0; i < priv->array_val->len; i++) {
				DfsmAstExpression *expression;
				GVariantType *child_type;

				expression = DFSM_AST_EXPRESSION (g_ptr_array_index (priv->array_val, i));

				child_type = dfsm_ast_expression_calculate_type (expression, environment);

				if (lg_child_type == NULL) {
					lg_child_type = g_variant_type_copy (child_type);
				} else if (g_variant_type_is_subtype_of (child_type, lg_child_type) == FALSE) {
					/* HACK: Just make the least-general supertype an any type. */
					g_variant_type_free (lg_child_type);
					lg_child_type = g_variant_type_copy (G_VARIANT_TYPE_ANY);
				}

				g_variant_type_free (child_type);
			}

			/* Wrap it in an array. If the array was empty, use the any type for child elements. */
			array_type = g_variant_type_new_array ((lg_child_type != NULL) ? lg_child_type : G_VARIANT_TYPE_ANY);
			g_variant_type_free (lg_child_type);

			return array_type;
		}
		case DFSM_AST_DATA_STRUCT: {
			GVariantType *struct_type;
			GPtrArray/*<GVariantType>*/ *child_types;
			guint i;

			/* Empty structs need special-casing. */
			if (priv->struct_val->len == 0) {
				return g_variant_type_copy (G_VARIANT_TYPE_UNIT);
			}

			/* Build an array of the types of the struct elements. */
			child_types = g_ptr_array_new_with_free_func ((GDestroyNotify) g_variant_type_free);

			for (i = 0; i < priv->struct_val->len; i++) {
				GVariantType *element_type;

				element_type = dfsm_ast_expression_calculate_type (g_ptr_array_index (priv->struct_val, i), environment);
				g_ptr_array_add (child_types, element_type);
			}

			struct_type = g_variant_type_new_tuple ((const GVariantType**) child_types->pdata, child_types->len);
			g_ptr_array_unref (child_types);

			return struct_type;
		}
		case DFSM_AST_DATA_VARIANT:
			return g_variant_type_copy (G_VARIANT_TYPE_VARIANT);
		case DFSM_AST_DATA_DICT: {
			GVariantType *dict_type, *entry_type, *lg_key_type = NULL, *lg_value_type = NULL;
			guint i;

			/* Empty dictionaries need special-casing. */
			if (priv->dict_val->len == 0) {
				return g_variant_type_copy (G_VARIANT_TYPE_DICTIONARY);
			}

			/* We need to find a least general supertype of all the types of keys and values in the dictionary. */
			for (i = 0; i < priv->dict_val->len; i++) {
				DfsmAstDictionaryEntry *entry;
				GVariantType *key_type, *value_type;

				entry = (DfsmAstDictionaryEntry*) g_ptr_array_index (priv->dict_val, i);

				/* Key */
				key_type = dfsm_ast_expression_calculate_type (entry->key, environment);

				if (lg_key_type == NULL) {
					lg_key_type = g_variant_type_copy (key_type);
				} else if (g_variant_type_is_subtype_of (key_type, lg_key_type) == FALSE) {
					/* HACK: Just make the least-general supertype a basic type. */
					g_variant_type_free (lg_key_type);
					lg_key_type = g_variant_type_copy (G_VARIANT_TYPE_BASIC);
				}

				g_variant_type_free (key_type);

				/* Value */
				value_type = dfsm_ast_expression_calculate_type (entry->value, environment);

				if (lg_value_type == NULL) {
					lg_value_type = g_variant_type_copy (value_type);
				} else if (g_variant_type_is_subtype_of (value_type, lg_value_type) == FALSE) {
					/* HACK: Just make the least-general supertype an any type. */
					g_variant_type_free (lg_value_type);
					lg_value_type = g_variant_type_copy (G_VARIANT_TYPE_ANY);
				}

				g_variant_type_free (value_type);
			}

			/* Build the dictionary type. */
			entry_type = g_variant_type_new_dict_entry (lg_key_type, lg_value_type);
			dict_type = g_variant_type_new_array (entry_type);
			g_variant_type_free (entry_type);

			g_variant_type_free (lg_value_type);
			g_variant_type_free (lg_key_type);

			return dict_type;
		}
		case DFSM_AST_DATA_UNIX_FD:
			return g_variant_type_copy (G_VARIANT_TYPE_UINT32);
		case DFSM_AST_DATA_VARIABLE:
			return dfsm_ast_variable_calculate_type (priv->variable_val, environment);
		default:
			g_assert_not_reached ();
	}
}

static GVariantType *
_calculate_type (DfsmAstDataStructure *self, DfsmEnvironment *environment, GError **error)
{
	/* Cache the type. */
	if (self->priv->variant_type == NULL) {
		GVariantType *new_type = NULL;

		/* If we have a type annotation, take that under consideration. Explode if the type we calculate doesn't match it. */
		if (self->priv->type_annotation != NULL) {
			GVariantType *calculated_type, *annotated_type;

			calculated_type = __calculate_type (self, environment);
			annotated_type = g_variant_type_new (self->priv->type_annotation);

			if (g_variant_type_is_subtype_of (annotated_type, calculated_type) == FALSE) {
				gchar *annotated_type_string, *calculated_type_string;

				annotated_type_string = g_variant_type_dup_string (annotated_type);
				calculated_type_string = g_variant_type_dup_string (calculated_type);

				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             _("Type mismatch between type annotation (‘%s’) and data structure type (‘%s’)."), annotated_type_string,
				             calculated_type_string);

				g_free (calculated_type_string);
				g_free (annotated_type_string);

				g_variant_type_free (annotated_type);

				return NULL;
			}

			new_type = annotated_type;

			g_variant_type_free (calculated_type);
		} else {
			/* Just calculate the type. */
			new_type = __calculate_type (self, environment);
		}

		/* Check it's a definite type. */
		if (g_variant_type_is_definite (new_type) == FALSE) {
			gchar *calculated_type_string;

			calculated_type_string = g_variant_type_dup_string (new_type);

			g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
			             _("Indefinitely typed data structure (probably needs a type annotation added): %s"), calculated_type_string);

			g_free (calculated_type_string);
			g_variant_type_free (new_type);

			return NULL;
		}

		/* Success! */
		self->priv->variant_type = new_type;
	}

	return g_variant_type_copy (self->priv->variant_type);
}

static void
dfsm_ast_data_structure_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstDataStructurePrivate *priv = DFSM_AST_DATA_STRUCTURE (node)->priv;
	GVariantType *expected_type;
	guint i;

	switch (priv->data_structure_type) {
		case DFSM_AST_DATA_BYTE:
		case DFSM_AST_DATA_BOOLEAN:
		case DFSM_AST_DATA_INT16:
		case DFSM_AST_DATA_UINT16:
		case DFSM_AST_DATA_INT32:
		case DFSM_AST_DATA_UINT32:
		case DFSM_AST_DATA_INT64:
		case DFSM_AST_DATA_UINT64:
		case DFSM_AST_DATA_DOUBLE:
		case DFSM_AST_DATA_STRING:
		case DFSM_AST_DATA_OBJECT_PATH:
		case DFSM_AST_DATA_SIGNATURE:
		case DFSM_AST_DATA_UNIX_FD:
			/* Nothing to do here */
			break;
		case DFSM_AST_DATA_VARIANT:
			/* Valid expression? */
			dfsm_ast_node_check (DFSM_AST_NODE (priv->variant_val), environment, error);

			if (*error != NULL) {
				return;
			}

			break;
		case DFSM_AST_DATA_ARRAY: {
			/* All entries valid? */
			for (i = 0; i < priv->array_val->len; i++) {
				DfsmAstExpression *expr;

				expr = g_ptr_array_index (priv->array_val, i);

				/* Valid expression? */
				dfsm_ast_node_check (DFSM_AST_NODE (expr), environment, error);

				if (*error != NULL) {
					return;
				}
			}

			break;
		}
		case DFSM_AST_DATA_STRUCT: {
			/* All entries valid? */
			for (i = 0; i < priv->struct_val->len; i++) {
				DfsmAstExpression *expr;

				expr = g_ptr_array_index (priv->struct_val, i);

				dfsm_ast_node_check (DFSM_AST_NODE (expr), environment, error);

				if (*error != NULL) {
					return;
				}
			}

			break;
		}
		case DFSM_AST_DATA_DICT: {
			/* All entries valid? */
			for (i = 0; i < priv->dict_val->len; i++) {
				DfsmAstDictionaryEntry *entry;

				/* Valid expressions? */
				entry = g_ptr_array_index (priv->dict_val, i);

				dfsm_ast_node_check (DFSM_AST_NODE (entry->key), environment, error);

				if (*error != NULL) {
					return;
				}

				dfsm_ast_node_check (DFSM_AST_NODE (entry->value), environment, error);

				if (*error != NULL) {
					return;
				}
			}

			/* NOTE: We can't check for duplicate keys here, since the expressions for the dict keys can't be evaluated yet. We have to
			 * allow duplicate keys at runtime, and allow the GVariant code to just use the value corresponding to the first key of any
			 * duplicate pair. */

			break;
		}
		case DFSM_AST_DATA_VARIABLE:
			/* Valid variable? */
			dfsm_ast_node_check (DFSM_AST_NODE (priv->variable_val), environment, error);

			if (*error != NULL) {
				return;
			}

			break;
		default:
			g_assert_not_reached ();
	}

	/* Calculating the type of the overall data structure will necessarily find any type errors. They're currently resolved by taking an any type
	 * as a supertype for things like arrays and dictionaries. */
	expected_type = _calculate_type (DFSM_AST_DATA_STRUCTURE (node), environment, error);

	if (*error != NULL) {
		return;
	}

	g_variant_type_free (expected_type);
}

/**
 * dfsm_ast_data_structure_new:
 * @data_structure_type: the type of the outermost data structure
 * @value: value of the data structure
 *
 * Create a new #DfsmAstDataStructure of type @data_structure_type and containing the given @value.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstDataStructure *
dfsm_ast_data_structure_new (DfsmAstDataStructureType data_structure_type, gpointer value)
{
	DfsmAstDataStructure *data_structure;
	DfsmAstDataStructurePrivate *priv;

	g_return_val_if_fail (data_structure_type == DFSM_AST_DATA_BOOLEAN || value != NULL, NULL);

	data_structure = g_object_new (DFSM_TYPE_AST_DATA_STRUCTURE, NULL);
	priv = data_structure->priv;

	switch (data_structure_type) {
		case DFSM_AST_DATA_BOOLEAN:
			priv->boolean_val = (GPOINTER_TO_UINT (value) == 1) ? TRUE : FALSE;
			break;
		case DFSM_AST_DATA_BYTE:
		case DFSM_AST_DATA_INT16:
		case DFSM_AST_DATA_UINT16:
		case DFSM_AST_DATA_INT32:
		case DFSM_AST_DATA_UINT32:
		case DFSM_AST_DATA_INT64:
		case DFSM_AST_DATA_UINT64:
		case DFSM_AST_DATA_DOUBLE:
			/* Parsed during pre_check_and_register() */
			priv->unparsed_string = g_strdup ((gchar*) value);
			break;
		case DFSM_AST_DATA_STRING:
			priv->string_val = g_strdup ((gchar*) value);
			break;
		case DFSM_AST_DATA_OBJECT_PATH:
			priv->object_path_val = g_strdup ((gchar*) value);
			break;
		case DFSM_AST_DATA_SIGNATURE:
			priv->signature_val = g_strdup ((gchar*) value);
			break;
		case DFSM_AST_DATA_ARRAY:
			priv->array_val = g_ptr_array_ref (value); /* array of DfsmAstExpressions */
			break;
		case DFSM_AST_DATA_STRUCT:
			priv->struct_val = g_ptr_array_ref (value); /* array of DfsmAstExpressions */
			break;
		case DFSM_AST_DATA_VARIANT:
			priv->variant_val = g_object_ref (value); /* DfsmAstExpression */
			break;
		case DFSM_AST_DATA_DICT:
			priv->dict_val = g_ptr_array_ref (value); /* array of DfsmAstDictionaryEntrys */
			break;
		case DFSM_AST_DATA_UNIX_FD:
			/* Note: not representable in the FSM language. */
			priv->unix_fd_val = 0;
			break;
		case DFSM_AST_DATA_VARIABLE:
			priv->variable_val = g_object_ref (value); /* DfsmAstVariable */
			break;
		default:
			g_assert_not_reached ();
	}

	priv->data_structure_type = data_structure_type;
	priv->weight = 0.0;

	return data_structure;
}

/**
 * dfsm_ast_data_structure_set_weight:
 * @self: a #DfsmAstDataStructure
 * @weight: weight of the structure for fuzzing
 *
 * Set the weight of the data structure. The @weight determines how “important” the data structure is for fuzzing. <code class="literal">1.0</code>
 * indicates no preference as to the structure's weight, more positive values mean more importance, and negative or zero values mean the structure
 * shouldn't be fuzzed at all.
 */
void
dfsm_ast_data_structure_set_weight (DfsmAstDataStructure *self, gdouble weight)
{
	g_return_if_fail (DFSM_IS_AST_DATA_STRUCTURE (self));

	/* Normalise the weight. */
	if (weight < 0.0) {
		weight = 0.0;
	}

	/* Some data structure types don't support fuzzing. */
	if (weight > 0.0) {
		switch (self->priv->data_structure_type) {
			case DFSM_AST_DATA_BYTE:
			case DFSM_AST_DATA_BOOLEAN:
			case DFSM_AST_DATA_INT16:
			case DFSM_AST_DATA_UINT16:
			case DFSM_AST_DATA_INT32:
			case DFSM_AST_DATA_UINT32:
			case DFSM_AST_DATA_INT64:
			case DFSM_AST_DATA_UINT64:
			case DFSM_AST_DATA_DOUBLE:
			case DFSM_AST_DATA_STRING:
			case DFSM_AST_DATA_OBJECT_PATH:
			case DFSM_AST_DATA_SIGNATURE:
			case DFSM_AST_DATA_ARRAY:
			case DFSM_AST_DATA_DICT:
			case DFSM_AST_DATA_VARIANT:
				/* No problems with fuzzing these. */
				break;
			case DFSM_AST_DATA_STRUCT:
				g_warning (_("Can't fuzz structures. Ignoring the indication to fuzz %p."), self);
				return;
			case DFSM_AST_DATA_UNIX_FD:
				g_warning (_("Can't fuzz Unix FDs. Ignoring the indication to fuzz %p."), self);
				return;
			case DFSM_AST_DATA_VARIABLE:
				g_warning (_("Can't fuzz variables. Ignoring the indication to fuzz %p."), self);
				return;
			default:
				g_assert_not_reached ();
		}
	}

	self->priv->weight = weight;
}

/**
 * dfsm_ast_data_structure_get_weight:
 * @self: a #DfsmAstDataStructure
 *
 * Get the fuzzing weight of this structure. For more information, see dfsm_ast_data_structure_set_weight().
 *
 * Return value: fuzzing weight of the data structure
 */
gdouble
dfsm_ast_data_structure_get_weight (DfsmAstDataStructure *self)
{
	g_return_val_if_fail (DFSM_IS_AST_DATA_STRUCTURE (self), 0.0);

	return self->priv->weight;
}

void
dfsm_ast_data_structure_set_type_annotation (DfsmAstDataStructure *self, const gchar *type_annotation)
{
	g_return_if_fail (DFSM_IS_AST_DATA_STRUCTURE (self));
	g_return_if_fail (type_annotation != NULL && *type_annotation != '\0');

	g_free (self->priv->type_annotation);
	self->priv->type_annotation = g_strdup (type_annotation);
}

/**
 * dfsm_ast_data_structure_get_nickname:
 * @self: a #DfsmAstDataStructure
 *
 * Get the user-provided nickname for this structure, or %NULL if none was set.
 *
 * Return value: (allow-none): nickname of the data structure, or %NULL
 */
const gchar *
dfsm_ast_data_structure_get_nickname (DfsmAstDataStructure *self)
{
	g_return_val_if_fail (DFSM_IS_AST_DATA_STRUCTURE (self), NULL);

	return self->priv->nickname;
}

void
dfsm_ast_data_structure_set_nickname (DfsmAstDataStructure *self, const gchar *nickname)
{
	g_return_if_fail (DFSM_IS_AST_DATA_STRUCTURE (self));
	g_return_if_fail (nickname == NULL || *nickname != '\0');

	g_free (self->priv->nickname);
	self->priv->nickname = g_strdup (nickname);
}

/**
 * dfsm_ast_data_structure_calculate_type:
 * @self: a #DfsmAstDataStructure
 * @environment: a #DfsmEnvironment containing all variables
 *
 * Calculate the type of the given data structure. In some cases this may not be a definite type, for example if the data structure is an empty
 * array, struct or dictionary. In most cases, however, the type will be definite.
 *
 * This assumes that the data structure has already been checked, and so this does not perform any type checking of its own.
 *
 * Return value: (transfer full): the type of the data structure
 */
GVariantType *
dfsm_ast_data_structure_calculate_type (DfsmAstDataStructure *self, DfsmEnvironment *environment)
{
	GVariantType *retval;

	g_return_val_if_fail (DFSM_IS_AST_DATA_STRUCTURE (self), NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);

	retval = _calculate_type (self, environment, NULL);
	g_assert (retval != NULL);

	return retval;
}

/* NOTE: Not thread safe. */
static gboolean enable_fuzzing = TRUE;

/**
 * dfsm_ast_data_structure_set_fuzzing_enabled:
 * @enable: %TRUE to enable fuzzing, %FALSE to disable it
 *
 * Set whether fuzzing should be performed on any AST data structures. If this is set to %TRUE, fuzzing will be performed on all data structures with
 * a positive weight. If this is set to %FALSE, fuzzing will be performed on no data structures, and they will all just take their default value.
 */
void
dfsm_ast_data_structure_set_fuzzing_enabled (gboolean enable)
{
	enable_fuzzing = enable;
}

static gboolean
should_be_fuzzed (DfsmAstDataStructure *self)
{
	return (enable_fuzzing == TRUE && self->priv->weight > 0.0) ? TRUE : FALSE;
}

static gint64
fuzz_signed_int (gint64 default_value, gint64 min_value, gint64 max_value)
{
	g_assert (min_value <= default_value && default_value <= max_value);

	DFSM_NONUNIFORM_DISTRIBUTION (4,
		SMALL_RANGE, 0.3, /* a number in the range [-5, 5] */
		DEFAULT, 0.3, /* keep our default value */
		BOUNDARY, 0.1, /* a boundary number for the given range */
		LARGE_RANGE, 0.3 /* a random integer in the given range */
	)
		case SMALL_RANGE:
			/* Number in the range [-5, 5]. */
			return g_random_int_range (-5, 6);
		case DEFAULT:
			/* Default value. */
			return default_value;
		case BOUNDARY:
			/* Boundary number. */
			if (g_random_boolean () == TRUE) {
				/* Lower boundary. */
				return min_value;
			} else {
				/* Upper boundary. */
				return max_value;
			}
		case LARGE_RANGE:
			/* Random int in the given range. If the range is large, we'll have to combine a g_random_int() call with a coin toss to
			 * determine the sign, since g_random_int() only returns 32-bit integers. */
			if (min_value >= G_MININT32 && max_value <= G_MAXINT32) {
				return g_random_int_range (min_value, max_value);
			} else {
				g_assert (min_value == G_MININT64 && max_value == G_MAXINT64);

				if (g_random_boolean () == TRUE) {
					return g_random_int ();
				} else {
					return (-1) - g_random_int (); /* shift it down by 1 so we don't cover 0 twice */
				}
			}
	DFSM_NONUNIFORM_DISTRIBUTION_END
}

static guint64
fuzz_unsigned_int (guint64 default_value, guint64 min_value, guint64 max_value)
{
	g_assert (min_value <= default_value && default_value <= max_value);

	DFSM_NONUNIFORM_DISTRIBUTION (4,
		SMALL_RANGE, 0.3, /* a number in the range [0, 10] */
		DEFAULT, 0.3, /* keep our default value */
		BOUNDARY, 0.1, /* a boundary number for the given range */
		LARGE_RANGE, 0.3 /* a random integer in the given range */
	)
		case SMALL_RANGE:
			/* Number in the range [0, 10]. */
			return g_random_int_range (0, 11);
		case DEFAULT:
			/* Default value. */
			return default_value;
		case BOUNDARY:
			/* Boundary number. */
			if (g_random_boolean () == TRUE) {
				/* Lower boundary. */
				return min_value;
			} else {
				/* Upper boundary. */
				return max_value;
			}
		case LARGE_RANGE:
			/* Random int in the given range. If the range is large, we'll have to combine two g_random_int() calls to get a 64-bit integer,
			 * since g_random_int() only returns 32-bit integers. */
			if (/*min_value >= 0 && */ max_value <= G_MAXINT32) {
				return g_random_int_range (min_value, max_value);
			} else if (/*min_value >= 0 && */ max_value <= G_MAXUINT32) {
				g_assert (min_value == 0 && max_value == G_MAXUINT32);

				return g_random_int ();
			} else {
				g_assert (min_value == 0 && max_value == G_MAXUINT64);

				return (((guint64) g_random_int () << 32) | (guint64) g_random_int ());
			}
	DFSM_NONUNIFORM_DISTRIBUTION_END
}

static void
find_random_block_with_separator (const gchar *input, gsize input_length /* bytes */, const gchar separator, const gchar **block_start,
                                  const gchar **block_end)
{
	guint num_separators = 0;
	gint start_separator, end_separator;
	const gchar *i;

	g_assert (input != NULL);

	/* Count the number of occurrences of the separator in the input string. */
	for (i = input; ((gsize) (i - input)) < input_length; i++) {
		if (*i == separator) {
			num_separators++;
		}
	}

	g_assert (num_separators > 0);

	/* Randomly choose two separator instances to be the start and end of the block. We also consider the start and end of the string as
	 * separators: this allows us to handle the situation of a single separator in the string. */
	start_separator = g_random_int_range (0, num_separators + 1);
	end_separator = (num_separators > 0) ? g_random_int_range (0, num_separators) : 0; /* sampling without replacement */

	if (start_separator > end_separator) {
		gint temp = start_separator;
		start_separator = end_separator;
		end_separator = temp;
	}

	/* Re-adjust the end separator so that both separators use the same numbering scheme, and are guaranteed to be different. */
	if (end_separator >= start_separator) {
		end_separator++;
	}

	/* Assign the start separator. start_separator == 0 means the start of the string. start_separator == num_separators means the end of the
	 * string. */
	for (i = input; start_separator >= 0 && ((gsize) (i - input)) < input_length; i++) {
		if (*i == separator) {
			start_separator--;
		}
	}

	*block_start = i;

	/* Assign the end separator. Same as above. */
	for (i = input; end_separator >= 0 && ((gsize) (i - input)) < input_length; i++) {
		if (*i == separator) {
			end_separator--;
		}
	}

	*block_end = i;
}

static const gchar random_block_separators[] = { '/', '.', ':', ',', ';', '=', '\n' };

static gsize
find_random_block (const gchar *input, gsize input_length /* bytes */, const gchar **block_start_out, const gchar **block_end_out)
{
	gboolean has_separator[G_N_ELEMENTS (random_block_separators)] = { FALSE, };
	guint j, num_separators_found = 0 /* number of _distinct_ separators found */, distribution;
	const gchar *i, *block_start, *block_end;

	g_assert (input != NULL);

	/* Bail quickly if the input is empty. */
	if (*input == '\0') {
		block_start = input;
		block_end = input;

		goto done;
	}

	/* We use a uniform distribution over all separators _which exist_. We consider these four characters to be common separators for blocks of
	 * text, which is true in the D-Bus world. Dots are used in interface names and slashes are used in object paths. Commas and colons are not
	 * specifically used in D-Bus, but are commonly used elsewhere in software. */
	for (i = input; num_separators_found < G_N_ELEMENTS (random_block_separators) && ((gsize) (i - input)) < input_length; i++) {
		for (j = 0; j < G_N_ELEMENTS (random_block_separators); j++) {
			if (*i == random_block_separators[j] && has_separator[j] == FALSE) {
				has_separator[j] = TRUE;
				num_separators_found++;
			}
		}
	}

	if (num_separators_found == 0) {
		guint32 start_offset;
		glong input_length_unicode = g_utf8_strlen (input, input_length);

		g_assert (input_length_unicode > 0);

		/* Give up on interesting separators and just choose a block of characters at random. */
		start_offset = g_random_int_range (0, input_length_unicode);
		block_start = g_utf8_offset_to_pointer (input, start_offset);
		block_end = g_utf8_offset_to_pointer (block_start, g_random_int_range (0, input_length_unicode - start_offset + 1));

		goto done;
	}

	/* Since we know that there's at least one instance of at least one of the separator characters in the input, randomly choose a separator
	 * character and find a block delimited by it. We do this by examining which separators were found, skipping over separators which weren't
	 * found, and choosing the first separator whose probability interval the distribution random variable falls into. */
	distribution = g_random_int ();

	for (j = 0; j < G_N_ELEMENTS (has_separator); j++) {
		if (has_separator[j] == TRUE) {
			if (distribution < G_MAXUINT32 / num_separators_found) {
				/* RV falls into this separator's probability interval. We're done. */
				find_random_block_with_separator (input, input_length, random_block_separators[j], &block_start, &block_end);
				goto done;
			}

			/* Decrement the RV to make the comparison next time round easier. */
			distribution -= G_MAXUINT32 / num_separators_found;
		}
	}

	g_assert_not_reached ();

done:
	g_assert (block_start <= block_end);
	g_assert (block_end <= input + input_length);
	g_assert ((gint32) g_utf8_get_char_validated (block_start, -1) >= 0);
	g_assert ((gint32) g_utf8_get_char_validated (block_end, -1) >= 0);

	*block_start_out = block_start;
	*block_end_out = block_end;

	return (block_end - block_start);
}

static void
generate_whitespace (gchar *buffer, gsize whitespace_length)
{
	const gchar whitespace_chars[] = {
		' ',
		'\t',
		'\n',
		'\r',
		'\v',
		'\f'
	};

	while (whitespace_length-- > 0) {
		/* NOTE: This could be sped up if necessary by generating a full 32 bits of randomness then splitting it, bitwise, into ~10 groups of
		 * three bits, which could each be used to index whitespace_chars. */
		buffer[whitespace_length] = whitespace_chars[g_random_int_range (0, G_N_ELEMENTS (whitespace_chars))];
	}
}

static gunichar
generate_character (void)
{
	gunichar output;

	DFSM_NONUNIFORM_DISTRIBUTION (3,
		ASCII, 0.5, /* any ASCII character (except NUL) */
		VALID_UNICODE, 0.4, /* any other valid Unicode character (except NUL) */
		INVALID_UNICODE, 0.1 /* any invalid Unicode character (such as the replacement character) */
	)
		case ASCII:
			/* ASCII. */
			output = g_random_int_range (0x01, 0xFF + 1); /* anything except NUL */

			break;
		case VALID_UNICODE:
			/* Valid Unicode. It would be impractical to list all assigned and valid code points here such that they all have a uniform
			 * probability of being chosen. Consequently, we just choose a random code point from planes 0, 1 and 2, and check whether it's
			 * assigned and valid. If not, we choose another. Note that we never choose NUL. */
			do {
				output = g_random_int_range (0x01, 0x2FFFF + 1);
			} while (g_unichar_isdefined (output) == FALSE || g_unichar_validate (output) == FALSE);

			break;
		case INVALID_UNICODE: {
			guint32 i;

			/* Invalid Unicode. We choose code points from:
			 *  • Private Use Area: U+E000–U+F8FF (6400 points).
			 *  • Supplementary Private Use Area A: U+F0000–U+FFFFD (65534 points).
			 *  • Supplementary Private Use Area B: U+100000–U+10FFFD (65534 points).
			 *  • Replacement Character: U+FFFD (1 point).
			 *
			 * We can't choose code points from:
			 *  • Surrogates Area: U+D800–U+DFFF (2048 points).
			 *  • Noncharacters: U+FFFE, U+FFFF, U+1FFFE, U+1FFFF, …, U+10FFFE, U+10FFFF; U+FDD0–U+FDEF (64 points).
			 * because g_utf8_validate() will reject them and cause assertion failures.
			 *
			 * This gives 137469 points in total.
			 */

			i = g_random_int_range (0, 137469);

			if (i < 6400) {
				/* Private Use Area */
				output = 0xE000 + i;
				break;
			}

			i -= 6400;

			if (i < 65534) {
				/* Supplementary Private Use Area A */
				output = 0xF0000 + i;
				break;
			}

			i -= 65534;

			if (i < 65534) {
				/* Supplementary Private Use Area B */
				output = 0x100000 + i;
				break;
			}

			/* Replacement Character */
			output = 0xFFFD;
			break;
		}
	DFSM_NONUNIFORM_DISTRIBUTION_END

	/* Sanity check. */
	g_assert (g_unichar_validate (output) == TRUE);

	return output;
}

static gchar *
fuzz_string (const gchar *default_value)
{
	gchar *fuzzy_string = NULL;
	gsize default_value_length, fuzzy_string_length = 0; /* both in bytes */

	default_value_length = strlen (default_value);

	/* If the default_value is the empty string, we:
	 *  • keep the empty string with probability 0.6; or
	 *  • generate a random string of random length with probability 0.4.
	 *
	 * There's no point then going on and applying any of the transformations above, since they're all designed to fuzz structured strings,
	 * which a randomly generated string most definitely isn't.
	 */

	if (default_value_length == 0) {
		if (DFSM_BIASED_COIN_FLIP (0.4)) {
			/* Generate a string between 1 and 256 characters (not bytes) long (inclusive). */
			guint32 i;
			gchar *j;

			i = g_random_int_range (1, 257);
			fuzzy_string = g_malloc (i * 6 /* max. byte length of a UTF-8 character */ + 1 /* nul terminator */);

			for (j = fuzzy_string; i > 0; i--) {
				/* Generate a character. To be more efficient, we should really be generating larger chunks at a time than this.
				 * Oh well. */
				j += g_unichar_to_utf8 (generate_character (), j);
			}

			/* Nul terminator */
			*j = '\0';

			fuzzy_string_length = j - fuzzy_string;
		}

		goto whitespace;
	}

	g_assert (default_value_length > 0);

	DFSM_NONUNIFORM_DISTRIBUTION (7,
		CASE_CHANGE, 0.1, /* change the case of some letters */
		REPLACE_LETTERS, 0.2, /* replace some letters with random replacements */
		DELETE_BLOCK, 0.1, /* delete a random block of text */
		OVERWRITE_BLOCK, 0.2, /* overwrite a random block of text with a random replacement */
		CLONE_BLOCK, 0.1, /* clone a random block of text to somewhere else in the string */
		SWAP_BLOCKS, 0.2, /* swap two random blocks of text */
		ADD_SEPARATORS, 0.1 /* replace letters with block separators at random locations */
		/* Additionally, and independently, we randomly add whitespace to the start and end of the string with probability 0.2. */
	)
		case CASE_CHANGE: {
			guint i;

			/* Case changing. Note that the probability of changing any given character position is ill-defined because it's dependent on
			 * the indices of the previously flipped characters, and also whether the character in question is ASCII. */
			fuzzy_string = g_strdup (default_value);
			fuzzy_string_length = default_value_length;

			i = g_random_int_range (0, fuzzy_string_length + 1);

			while (i < fuzzy_string_length) {
				if (g_ascii_isupper (fuzzy_string[i]) == TRUE) {
					fuzzy_string[i] = g_ascii_tolower (fuzzy_string[i]);
				} else if (g_ascii_islower (fuzzy_string[i]) == TRUE) {
					fuzzy_string[i] = g_ascii_toupper (fuzzy_string[i]);
				}

				i += g_random_int_range (i + 1, fuzzy_string_length + 1);
			}

			break;
		}
		case REPLACE_LETTERS: {
			guint old_i, i, j;
			glong default_value_length_unicode;
			gchar *temp;

			/* Letter replacement. As with case changing, the probability of replacing any given character position is ill-defined because
			 * it's dependent on the indices of the previously replaced characters. Note that we have to perform this operation in terms
			 * of Unicode characters, rather than bytes. */
			default_value_length_unicode = g_utf8_strlen (default_value, -1);
			fuzzy_string = g_malloc (default_value_length_unicode * 6 /* max. byte length of a UTF-8 character */ + 1);
			temp = fuzzy_string;

			old_i = 0;
			i = g_random_int_range (0, default_value_length_unicode + 1);

			while (i < default_value_length_unicode) {
				/* Copy the chunk between the previously replaced character and the next character to replace
				 * (at character offset i). */
				for (j = old_i; j < i; j++) {
					const gchar *next_char = g_utf8_find_next_char (default_value, NULL);

					while (default_value < next_char) {
						*(temp++) = *(default_value++);
					}
				}

				/* Replace character i. */
				temp += g_unichar_to_utf8 (generate_character (), temp);
				default_value = g_utf8_next_char (default_value);

				/* Choose the next character to replace. */
				old_i = i + 1;
				i = g_random_int_range (old_i, default_value_length_unicode + 1);
			}

			/* Copy the final chunk. */
			for (j = old_i; j < i; j++) {
				const gchar *next_char = g_utf8_find_next_char (default_value, NULL);

				while (default_value < next_char) {
					*(temp++) = *(default_value++);
				}
			}

			*temp = '\0';

			fuzzy_string_length = temp - fuzzy_string;

			break;
		}
		case DELETE_BLOCK: {
			const gchar *block_start, *block_end;
			gsize block_length;

			/* Block deletion. Find a random block and build a new string which doesn't include it. */
			block_length = find_random_block (default_value, default_value_length, &block_start, &block_end);

			fuzzy_string_length = default_value_length - block_length;
			fuzzy_string = g_malloc (fuzzy_string_length + 1);

			strncpy (fuzzy_string, default_value, block_start - default_value);
			strncpy (fuzzy_string + (block_start - default_value), block_end, default_value + default_value_length - block_end);
			fuzzy_string[fuzzy_string_length] = '\0';

			break;
		}
		case OVERWRITE_BLOCK: {
			gchar *block_start, *block_end, *i;

			/* Block overwriting. Find a random block and build a new string which replaces it with the same number of bytes. */
			fuzzy_string = g_strdup (default_value);
			fuzzy_string_length = default_value_length;

			find_random_block (fuzzy_string, fuzzy_string_length, (const gchar**) &block_start, (const gchar**) &block_end);

			for (i = block_start; i + 8 <= block_end;) {
				*(i++) = 'd';
				*(i++) = 'e';
				*(i++) = 'a';
				*(i++) = 'd';
				*(i++) = 'b';
				*(i++) = 'e';
				*(i++) = 'e';
				*(i++) = 'f';
			}

			switch (block_end - i) {
				case 7:
					*(i++) = 'f';
				case 6:
					*(i++) = 'u';
				case 5:
					*(i++) = 'z';
				case 4:
					*(i++) = 'z';
				case 3:
					*(i++) = 'i';
				case 2:
					*(i++) = 'n';
				case 1:
					*(i++) = 'g';
				case 0:
					/* Don't potentially overwrite the nul terminator */
					break;
				default:
					g_assert_not_reached ();
			}

			break;
		}
		case CLONE_BLOCK: {
			const gchar *block_start, *block_end;
			gsize block_length;

			/* Block cloning. Find a random block and clone it in the same position. */
			block_length = find_random_block (default_value, default_value_length, &block_start, &block_end);

			fuzzy_string_length = default_value_length + block_length;
			fuzzy_string = g_malloc (fuzzy_string_length + 1);

			strncpy (fuzzy_string, default_value, block_end - default_value);
			strncpy (fuzzy_string + (block_end - default_value), block_start, block_length);
			strncpy (fuzzy_string + (block_end - default_value) + block_length, block_end,
			         default_value + default_value_length - block_end);
			fuzzy_string[fuzzy_string_length] = '\0';

			break;
		}
		case SWAP_BLOCKS: {
			const gchar *block1_start, *block2_start, *block1_end, *block2_end;
			gchar *i;
			gsize block1_length, block2_length;

			/* Block swapping. Find two random blocks and swap them. We have to be careful to make sure they don't overlap, so we take the
			 * second block from the larger of the remaining portions after choosing the first block. */
			block1_length = find_random_block (default_value, default_value_length, &block1_start, &block1_end);

			if (block1_start - default_value > default_value + default_value_length - block1_end) {
				const gchar *temp_start, *temp_end;
				gsize temp_length;

				temp_length = find_random_block (default_value, block1_start - default_value, &temp_start, &temp_end);

				/* Ensure block1 is always < block2. */
				block2_start = block1_start;
				block2_end = block1_end;
				block2_length = block1_length;
				block1_start = temp_start;
				block1_end = temp_end;
				block1_length = temp_length;
			} else {
				block2_length = find_random_block (block1_end, default_value + default_value_length - block1_end,
				                                   &block2_start, &block2_end);
			}

			/* Build the output string. */
			fuzzy_string_length = default_value_length;
			fuzzy_string = g_malloc (fuzzy_string_length + 1);
			i = fuzzy_string;

			strncpy (i, default_value, block1_start - default_value);
			i += block1_start - default_value;
			strncpy (i, block2_start, block2_length);
			i += block2_length;
			strncpy (i, block1_end, block2_start - block1_end);
			i += block2_start - block1_end;
			strncpy (i, block1_start, block1_length);
			i += block1_length;
			strncpy (i, block2_end, default_value + default_value_length - block2_end);

			fuzzy_string[fuzzy_string_length] = '\0';

			break;
		}
		case ADD_SEPARATORS: {
			guint old_i, i, j;
			glong default_value_length_unicode;
			gchar *temp;

			/* Letter replacement with block separators. Much the same as with normal letter replacement, except we replace letters with
			 * block separators only. The separators are only ever 1 byte long, so we can allocate a fuzzy string of the same length as
			 * the original. */
			default_value_length_unicode = g_utf8_strlen (default_value, -1);
			fuzzy_string = g_malloc (default_value_length + 1);
			temp = fuzzy_string;

			old_i = 0;
			i = g_random_int_range (0, default_value_length_unicode + 1);

			while (i < default_value_length_unicode) {
				guint sep;

				/* Copy the chunk between the previously replaced character and the next character to replace
				 * (at character offset i). */
				for (j = old_i; j < i; j++) {
					const gchar *next_char = g_utf8_find_next_char (default_value, NULL);

					while (default_value < next_char) {
						*(temp++) = *(default_value++);
					}
				}

				/* Replace character i. */
				sep = g_random_int_range (0, G_N_ELEMENTS (random_block_separators));
				*(temp++) = random_block_separators[sep];
				default_value = g_utf8_next_char (default_value);

				/* Choose the next character to replace. */
				old_i = i + 1;
				i = g_random_int_range (old_i, default_value_length_unicode + 1);
			}

			/* Copy the final chunk. */
			for (j = old_i; j < i; j++) {
				const gchar *next_char = g_utf8_find_next_char (default_value, NULL);

				while (default_value < next_char) {
					*(temp++) = *(default_value++);
				}
			}

			*temp = '\0';

			fuzzy_string_length = temp - fuzzy_string;

			break;
		}
	DFSM_NONUNIFORM_DISTRIBUTION_END

whitespace:
	/* Whitespace addition. */
	if (DFSM_BIASED_COIN_FLIP (0.2)) {
		gchar *temp;
		gsize prefix_length = 0, suffix_length = 0;

		if (g_random_boolean () == TRUE) {
			/* Add whitespace as a prefix. */
			prefix_length = g_random_int_range (1, 6);
		}

		if (g_random_boolean () == TRUE) {
			/* Independently add whitespace to the end of the fuzzy string. */
			suffix_length = g_random_int_range (1, 6);
		}

		/* Move the fuzzy string to a larger chunk of memory with space for the whitespace. */
		temp = g_malloc (prefix_length + fuzzy_string_length + suffix_length + 1);
		strncpy (temp + prefix_length, fuzzy_string, fuzzy_string_length);
		temp[prefix_length + fuzzy_string_length + suffix_length] = '\0';

		/* Generate some whitespace to fill the gaps. */
		if (prefix_length > 0) {
			generate_whitespace (temp + 0, prefix_length);
		}

		if (suffix_length > 0) {
			generate_whitespace (temp + prefix_length + fuzzy_string_length, suffix_length);
		}

		/* Store the new string. */
		g_free (fuzzy_string);
		fuzzy_string = temp;
		fuzzy_string_length += prefix_length + suffix_length;
	}

	if (fuzzy_string == NULL) {
		fuzzy_string = g_strdup (default_value);
		fuzzy_string_length = default_value_length;
	}

	/* Sanity check. */
	g_assert (fuzzy_string != NULL &&
	          fuzzy_string_length == strlen (fuzzy_string) && g_utf8_validate (fuzzy_string, fuzzy_string_length, NULL) == TRUE);

	return fuzzy_string;
}

static gchar *
fuzz_object_path (const gchar *default_value)
{
	gchar *output;

	DFSM_NONUNIFORM_DISTRIBUTION (2,
		DEFAULT, 0.7, /* default value */
		APPENDED, 0.3 /* append a digit to the path */
	)
		case DEFAULT:
			output = g_strdup (default_value);
			break;
		case APPENDED:
			output = g_strdup_printf ("%s%u", default_value, g_random_int_range (0, 100));
			break;
	DFSM_NONUNIFORM_DISTRIBUTION_END

	/* Sanity check. */
	g_assert (g_variant_is_object_path (output) == TRUE);

	return output;
}

static GVariantType *
generate_basic_type_signature (void)
{
	GVariantType *type_signature;

	/* Generate a basic type signature. */
	DFSM_NONUNIFORM_DISTRIBUTION (12,
		BOOLEAN, 0.05,
		BYTE, 0.05,
		INT16, 0.1,
		UINT16, 0.1,
		INT32, 0.1,
		UINT32, 0.1,
		INT64, 0.1,
		UINT64, 0.1,
		DOUBLE, 0.1,
		STRING, 0.1,
		OBJECT_PATH, 0.05,
		SIGNATURE, 0.05
	)
#define BASIC_TYPE(T) \
		case T: \
			type_signature = g_variant_type_copy (G_VARIANT_TYPE_##T); \
			break;
		BASIC_TYPE (BOOLEAN)
		BASIC_TYPE (BYTE)
		BASIC_TYPE (INT16)
		BASIC_TYPE (UINT16)
		BASIC_TYPE (INT32)
		BASIC_TYPE (UINT32)
		BASIC_TYPE (INT64)
		BASIC_TYPE (UINT64)
		BASIC_TYPE (DOUBLE)
		BASIC_TYPE (STRING)
		BASIC_TYPE (OBJECT_PATH)
		BASIC_TYPE (SIGNATURE)
#undef BASIC_TYPE
	DFSM_NONUNIFORM_DISTRIBUTION_END

	return type_signature;
}

static GVariantType *
generate_type_signature (void)
{
	GVariantType *type_signature;

	/* Recursively generate a type signature. */
	DFSM_NONUNIFORM_DISTRIBUTION (5,
		BASIC, 0.6,
		VARIANT, 0.1,
		ARRAY, 0.1,
		TUPLE, 0.1,
		DICTIONARY, 0.1
	)
		case BASIC:
			type_signature = generate_basic_type_signature ();
			break;
		case VARIANT:
			type_signature = g_variant_type_copy (G_VARIANT_TYPE_VARIANT);
			break;
		case ARRAY: {
			GVariantType *element_type = generate_type_signature ();
			type_signature = g_variant_type_new_array (element_type);
			g_variant_type_free (element_type);

			break;
		}
		case TUPLE: {
			guint i;
			GPtrArray/*<GVariantType>*/ *element_types;

			i = g_random_int_range (0, 6);
			element_types = g_ptr_array_sized_new (i);
			g_ptr_array_set_free_func (element_types, (GDestroyNotify) g_variant_type_free);

			while (i-- > 0) {
				g_ptr_array_add (element_types, generate_type_signature ());
			}

			type_signature = g_variant_type_new_tuple ((const GVariantType* const*) element_types->pdata, element_types->len);

			g_ptr_array_unref (element_types);

			break;
		}
		case DICTIONARY: {
			GVariantType *key_type, *value_type, *entry_type;

			key_type = generate_basic_type_signature ();
			value_type = generate_type_signature ();
			entry_type = g_variant_type_new_dict_entry (key_type, value_type);

			type_signature = g_variant_type_new_array (entry_type);

			g_variant_type_free (entry_type);
			g_variant_type_free (value_type);
			g_variant_type_free (key_type);

			break;
		}
	DFSM_NONUNIFORM_DISTRIBUTION_END

	return type_signature;
}

static gchar *
fuzz_type_signature (const gchar *default_value)
{
	gchar *output;

	DFSM_NONUNIFORM_DISTRIBUTION (2,
		DEFAULT, 0.6, /* default value */
		GENERATED, 0.4 /* a randomly generated type signature */
	)
		case DEFAULT:
			/* Default value. */
			output = g_strdup (default_value);

			break;
		case GENERATED: {
			/* Generated type signature. */
			GVariantType *type_signature = generate_type_signature ();
			output = g_variant_type_dup_string (type_signature);
			g_variant_type_free (type_signature);

			break;
		}
	DFSM_NONUNIFORM_DISTRIBUTION_END

	/* Sanity check. */
	g_assert (g_variant_is_signature (output) == TRUE);

	return output;
}

/* Evaluate a data structure, ensuring that it's fuzzed in the process. A bit hacky. */
static GVariant *
fuzz_data_structure (DfsmAstDataStructure *data_structure, DfsmEnvironment *environment)
{
	gdouble old_weight;
	GVariant *variant;

	/* Temporarily set the child data structure's fuzzing weight to 1.0 (if it's not already) to ensure that evaluating it again will mutate it. */
	old_weight = data_structure->priv->weight;
	data_structure->priv->weight = MAX (1.0, old_weight);

	variant = dfsm_ast_data_structure_to_variant (data_structure, environment);

	data_structure->priv->weight = old_weight;

	return variant;
}

/**
 * dfsm_ast_data_structure_to_variant:
 * @self: a #DfsmAstDataStructure
 * @environment: a #DfsmEnvironment containing all variables
 *
 * Convert the data structure given by @self to a #GVariant in the given @environment.
 *
 * This assumes that the data structure has been successfully checked by dfsm_ast_node_check() beforehand. It is an error to call this function
 * otherwise.
 *
 * Return value: (transfer full): the non-floating #GVariant representation of the data structure
 */
GVariant *
dfsm_ast_data_structure_to_variant (DfsmAstDataStructure *self, DfsmEnvironment *environment)
{
	DfsmAstDataStructurePrivate *priv;

	g_return_val_if_fail (DFSM_IS_AST_DATA_STRUCTURE (self), NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);

	priv = self->priv;

	/* NOTE: We have to sink all floating references from here to guarantee that we always return a value of the same floatiness. The alternative
	 * is to always return a floating reference, but that would require modifying dfsm_ast_variable_to_variant() to somehow return a floating
	 * reference, which in turn probably means deep-copying the variable value. Ouch. */

	switch (priv->data_structure_type) {
		case DFSM_AST_DATA_BYTE: {
			guchar byte_val = priv->byte_val;

			if (should_be_fuzzed (self) == TRUE) {
				byte_val = fuzz_unsigned_int (byte_val, 0, UCHAR_MAX);
			}

			return g_variant_ref_sink (g_variant_new_byte (byte_val));
		}
		case DFSM_AST_DATA_BOOLEAN: {
			gboolean boolean_val = priv->boolean_val;

			if (should_be_fuzzed (self) == TRUE) {
				DFSM_NONUNIFORM_DISTRIBUTION (2,
					DEFAULT, 0.6, /* keep the default value */
					FLIP, 0.4 /* flip the default value */
				)
					case DEFAULT:
						break;
					case FLIP:
						boolean_val = !boolean_val;
						break;
				DFSM_NONUNIFORM_DISTRIBUTION_END
			}

			return g_variant_ref_sink (g_variant_new_boolean (boolean_val));
		}
		case DFSM_AST_DATA_INT16: {
			gint16 int16_val = priv->int16_val;

			if (should_be_fuzzed (self) == TRUE) {
				int16_val = fuzz_signed_int (int16_val, G_MININT16, G_MAXINT16);
			}

			return g_variant_ref_sink (g_variant_new_int16 (int16_val));
		}
		case DFSM_AST_DATA_UINT16: {
			guint16 uint16_val = priv->uint16_val;

			if (should_be_fuzzed (self) == TRUE) {
				uint16_val = fuzz_unsigned_int (uint16_val, 0, G_MAXUINT16);
			}

			return g_variant_ref_sink (g_variant_new_uint16 (uint16_val));
		}
		case DFSM_AST_DATA_INT32: {
			gint32 int32_val = priv->int32_val;

			if (should_be_fuzzed (self) == TRUE) {
				int32_val = fuzz_signed_int (int32_val, G_MININT32, G_MAXINT32);
			}

			return g_variant_ref_sink (g_variant_new_int32 (int32_val));
		}
		case DFSM_AST_DATA_UINT32: {
			guint32 uint32_val = priv->uint32_val;

			if (should_be_fuzzed (self) == TRUE) {
				uint32_val = fuzz_unsigned_int (uint32_val, 0, G_MAXUINT32);
			}

			return g_variant_ref_sink (g_variant_new_uint32 (uint32_val));
		}
		case DFSM_AST_DATA_INT64: {
			gint64 int64_val = priv->int64_val;

			if (should_be_fuzzed (self) == TRUE) {
				int64_val = fuzz_signed_int (int64_val, G_MININT64, G_MAXINT64);
			}

			return g_variant_ref_sink (g_variant_new_int64 (int64_val));
		}
		case DFSM_AST_DATA_UINT64: {
			guint64 uint64_val = priv->uint64_val;

			if (should_be_fuzzed (self) == TRUE) {
				uint64_val = fuzz_unsigned_int (uint64_val, 0, G_MAXUINT64);
			}

			return g_variant_ref_sink (g_variant_new_uint64 (uint64_val));
		}
		case DFSM_AST_DATA_DOUBLE: {
			gdouble double_val = priv->double_val;

			if (should_be_fuzzed (self) == TRUE) {
				DFSM_NONUNIFORM_DISTRIBUTION (3,
					SMALL_RANGE, 0.3, /* a number in the range [-5.0, 5.0) */
					DEFAULT, 0.3, /* keep our default value */
					LARGE_RANGE, 0.4 /* a random double in the given range */
				)
					case SMALL_RANGE:
						/* Number in the range [-5.0, 5.0). */
						double_val = g_random_double_range (-5.0, 5.0);
						break;
					case DEFAULT:
						/* Default value. */
						double_val = priv->double_val;
						break;
					case LARGE_RANGE:
						/* Random double in the maximum range. */
						double_val = g_random_double_range (-G_MAXDOUBLE, G_MAXDOUBLE);
						break;
				DFSM_NONUNIFORM_DISTRIBUTION_END
			}

			return g_variant_ref_sink (g_variant_new_double (double_val));
		}
		case DFSM_AST_DATA_STRING: {
			GVariantType *data_structure_type;
			GVariant *variant;
			gchar *fuzzed_val = priv->string_val;

			data_structure_type = dfsm_ast_data_structure_calculate_type (self, environment);

			/* Slight irregularity: if we've calculated the data structure type to be an object path or D-Bus signature (i.e. because the
			 * user added a type annotation), we need to create a GVariant of the appropriate type. */
			if (g_variant_type_equal (data_structure_type, G_VARIANT_TYPE_STRING) == TRUE) {
				if (should_be_fuzzed (self) == TRUE) {
					fuzzed_val = fuzz_string (priv->string_val);
				}

				variant = g_variant_new_string (fuzzed_val);
			} else if (g_variant_type_equal (data_structure_type, G_VARIANT_TYPE_OBJECT_PATH) == TRUE) {
				if (should_be_fuzzed (self) == TRUE) {
					fuzzed_val = fuzz_object_path (priv->string_val);
				}

				variant = g_variant_new_object_path (fuzzed_val);
			} else if (g_variant_type_equal (data_structure_type, G_VARIANT_TYPE_SIGNATURE) == TRUE) {
				if (should_be_fuzzed (self) == TRUE) {
					fuzzed_val = fuzz_type_signature (priv->string_val);
				}

				variant = g_variant_new_signature (fuzzed_val);
			} else {
				g_assert_not_reached ();
			}

			if (should_be_fuzzed (self) == TRUE) {
				g_free (fuzzed_val);
			}

			g_variant_type_free (data_structure_type);

			return g_variant_ref_sink (variant);
		}
		case DFSM_AST_DATA_OBJECT_PATH: {
			GVariant *variant;

			if (should_be_fuzzed (self) == TRUE) {
				gchar *fuzzed_val = fuzz_object_path (priv->object_path_val);
				variant = g_variant_new_object_path (fuzzed_val);
				g_free (fuzzed_val);
			} else {
				variant = g_variant_new_object_path (priv->object_path_val);
			}

			return g_variant_ref_sink (variant);
		}
		case DFSM_AST_DATA_SIGNATURE: {
			GVariant *variant;

			if (should_be_fuzzed (self) == TRUE) {
				gchar *fuzzed_val = fuzz_type_signature (priv->signature_val);
				variant = g_variant_new_signature (fuzzed_val);
				g_free (fuzzed_val);
			} else {
				variant = g_variant_new_signature (priv->signature_val);
			}

			return g_variant_ref_sink (variant);
		}
		case DFSM_AST_DATA_ARRAY: {
			GVariantType *data_structure_type;
			GVariantBuilder builder;
			guint i, effective_array_length;

			/* Fuzzing for arrays takes several forms, each of which are decided by independent probabilities for each array element
			 * individually:
			 *  • Cloning array elements happens with probability 0.2.
			 *  • Deleting array elements happens with probability 0.2.
			 *  • Cloning and mutating array elements happens with probability 0.4.
			 *
			 * Notably, the amount of fuzzing (of any type) on a given array element is affected by the fuzzing weight of the expression
			 * for the array element.
			 *
			 * Additionally, there is a global 0.05 chance of an array being emptied completely, regardless of what's happened to its
			 * individual elements.
			 */

			data_structure_type = dfsm_ast_data_structure_calculate_type (self, environment);
			g_variant_builder_init (&builder, data_structure_type);
			g_variant_type_free (data_structure_type);

			/* Delete all entries? */
			effective_array_length = (should_be_fuzzed (self) == FALSE || DFSM_BIASED_COIN_FLIP (0.95)) ? priv->array_val->len : 0;

			for (i = 0; i < effective_array_length; i++) {
				GVariant *child_value;
				DfsmAstExpression *child_expression;
				gdouble child_expression_weight;

				/* Evaluate the child expression to get a GVariant value. */
				child_expression = (DfsmAstExpression*) g_ptr_array_index (priv->array_val, i);
				child_expression_weight = MAX (1.0, dfsm_ast_expression_calculate_weight (child_expression));

				/* Delete this element? */
				if (should_be_fuzzed (self) && DFSM_BIASED_COIN_FLIP (0.2 * child_expression_weight)) {
					continue;
				}

				child_value = dfsm_ast_expression_evaluate (child_expression, environment);

				/* Add it to the growing GVariant array. */
				g_variant_builder_add_value (&builder, child_value);

				/* Clone this element? */
				if (should_be_fuzzed (self) && DFSM_BIASED_COIN_FLIP (0.2 * child_expression_weight)) {
					g_variant_builder_add_value (&builder, child_value);
				}

				g_variant_unref (child_value);

				/* Clone and mutate the element?  We can only do this if the child expression is a data structure expression. */
				if (should_be_fuzzed (self) && DFSM_IS_AST_EXPRESSION_DATA_STRUCTURE (child_expression) &&
				    DFSM_BIASED_COIN_FLIP (0.4 * child_expression_weight)) {
					DfsmAstDataStructure *child_data_structure;

					child_data_structure =
						dfsm_ast_expression_data_structure_get_data_structure (
							DFSM_AST_EXPRESSION_DATA_STRUCTURE (child_expression));

					/* Add the mutated value. */
					child_value = fuzz_data_structure (child_data_structure, environment);
					g_variant_builder_add_value (&builder, child_value);
					g_variant_unref (child_value);
				}
			}

			return g_variant_ref_sink (g_variant_builder_end (&builder));
		}
		case DFSM_AST_DATA_STRUCT: {
			GVariantType *data_structure_type;
			GVariantBuilder builder;
			guint i;

			/* Note: Fuzzing structs doesn't make sense, so we ignore any fuzzing here. */

			data_structure_type = dfsm_ast_data_structure_calculate_type (self, environment);
			g_variant_builder_init (&builder, data_structure_type);
			g_variant_type_free (data_structure_type);

			for (i = 0; i < priv->struct_val->len; i++) {
				GVariant *child_value;
				DfsmAstExpression *child_expression;

				/* Evaluate the child expression to get a GVariant value. */
				child_expression = (DfsmAstExpression*) g_ptr_array_index (priv->struct_val, i);
				child_value = dfsm_ast_expression_evaluate (child_expression, environment);

				/* Add it to the growing GVariant struct. */
				g_variant_builder_add_value (&builder, child_value);

				g_variant_unref (child_value);
			}

			return g_variant_ref_sink (g_variant_builder_end (&builder));
		}
		case DFSM_AST_DATA_VARIANT: {
			GVariant *default_child_value, *child_value, *variant_value;

			/* Fuzzing for variants can only be done in one way: fuzzing the type of the variant's value.
			 *  • Changing the type of the variant's value happens with probability 0.2.
			 *  • The value remains unchanged (i.e. with its default type) with probability 0.8.
			 *
			 * In the first case, we just choose the first basic type which isn't the default value's type. It's too complex and
			 * time-intensive to compute random values of arbitrary type, especially since the client code which unpacks the variant
			 * will almost certainly either handle all variant types, or only the one it's expecting. (So to test it, we just need to
			 * choose a variant type it wasn't expecting; i.e. any type other than the default value's type.)
			 *
			 * In the second case, fuzzing of the default value itself is performed by marking the default value as fuzzable, rather
			 * than by marking the enclosing variant as fuzzable. */

			default_child_value = dfsm_ast_expression_evaluate (priv->variant_val, environment);

			if (should_be_fuzzed (self) == TRUE && DFSM_BIASED_COIN_FLIP (0.2)) {
				/* Choose an arbitrary type and generate a value for it. See explanation above. */
				if (g_variant_type_equal (g_variant_get_type (default_child_value), G_VARIANT_TYPE_UINT32) == TRUE) {
					child_value = g_variant_ref_sink (g_variant_new_string (fuzz_string ("")));
				} else {
					child_value = g_variant_ref_sink (g_variant_new_uint32 (fuzz_unsigned_int (0, 0, G_MAXUINT32)));
				}
			} else {
				/* Leave the value unchanged. */
				child_value = g_variant_ref (default_child_value);
			}

			variant_value = g_variant_ref_sink (g_variant_new_variant (child_value));

			g_variant_unref (child_value);
			g_variant_unref (default_child_value);

			return variant_value;
		}
		case DFSM_AST_DATA_DICT: {
			GVariantType *data_structure_type;
			GVariantBuilder builder;
			guint i, effective_dict_length;

			/* Fuzzing for dictionaries takes several forms, each of which are decided by independent probabilities for each dict. entry
			 * individually:
			 *  • Deleting dict. entries happens with probability 0.2.
			 *  • Cloning and mutating dict. entries (keys only) happens with probability 0.3.
			 *  • Cloning and mutating dict. entries (keys and values) happens with probability 0.6.
			 *
			 * Notably, the amount of fuzzing (of any type) on a given dict. entry is affected by the fuzzing weight of the expression
			 * for the entry.
			 *
			 * Additionally, there is a global 0.05 chance of an array being emptied completely, regardless of what's happened to its
			 * individual elements.
			 */

			data_structure_type = dfsm_ast_data_structure_calculate_type (self, environment);
			g_variant_builder_init (&builder, data_structure_type);

			/* Delete all entries? */
			effective_dict_length = (should_be_fuzzed (self) == FALSE || DFSM_BIASED_COIN_FLIP (0.95)) ? priv->dict_val->len : 0;

			for (i = 0; i < effective_dict_length; i++) {
				GVariant *key_value, *value_value;
				DfsmAstDictionaryEntry *dict_entry;
				gdouble key_weight, value_weight;

				/* Evaluate the child expressions to get GVariant values. */
				dict_entry = (DfsmAstDictionaryEntry*) g_ptr_array_index (priv->dict_val, i);

				/* Clamp the weights to be positive. */
				key_weight = MAX (1.0, dfsm_ast_expression_calculate_weight (dict_entry->key));
				value_weight = MAX (1.0, dfsm_ast_expression_calculate_weight (dict_entry->value));

				/* Delete this entry? */
				if (should_be_fuzzed (self) && DFSM_BIASED_COIN_FLIP (0.2 * key_weight)) {
					continue;
				}

				key_value = dfsm_ast_expression_evaluate (dict_entry->key, environment);
				value_value = dfsm_ast_expression_evaluate (dict_entry->value, environment);

				/* Add them to the growing GVariant dict. */
				g_variant_builder_open (&builder, g_variant_type_element (data_structure_type));
				g_variant_builder_add_value (&builder, key_value);
				g_variant_builder_add_value (&builder, value_value);
				g_variant_builder_close (&builder);

				/* Clone and mutate the entry?  We can only do this if the child expressions are data structure expressions. */
				if (should_be_fuzzed (self) && DFSM_IS_AST_EXPRESSION_DATA_STRUCTURE (dict_entry->key) &&
				    DFSM_IS_AST_EXPRESSION_DATA_STRUCTURE (dict_entry->value) &&
				    DFSM_BIASED_COIN_FLIP (0.6 * key_weight)) {
					DfsmAstDataStructure *key_data_structure, *value_data_structure;

					key_data_structure =
						dfsm_ast_expression_data_structure_get_data_structure (
							DFSM_AST_EXPRESSION_DATA_STRUCTURE (dict_entry->key));
					value_data_structure =
						dfsm_ast_expression_data_structure_get_data_structure (
							DFSM_AST_EXPRESSION_DATA_STRUCTURE (dict_entry->value));

					g_variant_unref (key_value);
					key_value = fuzz_data_structure (key_data_structure, environment);

					if (DFSM_BIASED_COIN_FLIP (0.5 * value_weight)) {
						/* Mutate the value as well as the key. */
						g_variant_unref (value_value);
						value_value = fuzz_data_structure (value_data_structure, environment);
					}

					/* Add the mutated entry. */
					g_variant_builder_open (&builder, g_variant_type_element (data_structure_type));
					g_variant_builder_add_value (&builder, key_value);
					g_variant_builder_add_value (&builder, value_value);
					g_variant_builder_close (&builder);
				}

				g_variant_unref (value_value);
				g_variant_unref (key_value);
			}

			g_variant_type_free (data_structure_type);

			return g_variant_ref_sink (g_variant_builder_end (&builder));
		}
		case DFSM_AST_DATA_UNIX_FD:
			/* Note: Fuzzing Unix FDs isn't currently supported, so we ignore any fuzzing here. */
			return g_variant_ref_sink (g_variant_new_uint32 (priv->unix_fd_val));
		case DFSM_AST_DATA_VARIABLE:
			/* Note: Fuzzing variables isn't currently supported, so we ignore any fuzzing here. */
			return dfsm_ast_variable_to_variant (priv->variable_val, environment);
		default:
			g_assert_not_reached ();
	}
}

/**
 * dfsm_ast_data_structure_set_from_variant:
 * @self: a #DfsmAstDataStructure
 * @environment: a #DfsmEnvironment containing all variables
 * @new_value: the #GVariant value to assign to @self
 *
 * Set the given @self's value in @environment to the #GVariant value given in @new_value. This will recursively assign to child data
 * structures inside the data structure (e.g. if the data structure is an array of variables, each of the variables will be assigned to).
 *
 * It's an error to call this function with a data structure which isn't comprised entirely of variables or structures of them. Similarly, it's an
 * error to call this function with a @new_value which doesn't match the data structure's type and number of elements. These conditions can be checked
 * using dfsm_ast_data_structure_is_variable().
 */
void
dfsm_ast_data_structure_set_from_variant (DfsmAstDataStructure *self, DfsmEnvironment *environment, GVariant *new_value)
{
	DfsmAstDataStructurePrivate *priv;

	g_return_if_fail (DFSM_IS_AST_DATA_STRUCTURE (self));
	g_return_if_fail (DFSM_IS_ENVIRONMENT (environment));
	g_return_if_fail (new_value != NULL);

	priv = self->priv;

	switch (priv->data_structure_type) {
		case DFSM_AST_DATA_ARRAY: {
			guint i;

			/* We can safely assume that the array and the variant have the same type, length, etc. since that's all been checked
			 * before. */
			for (i = 0; i < priv->array_val->len; i++) {
				GVariant *child_variant;
				DfsmAstExpression *child_expression;

				/* We assume that the child_expression is a DFSM_AST_EXPRESSION_DATA_STRUCTURE and extract its data structure to
				 * recursively assign to. */
				child_expression = DFSM_AST_EXPRESSION (g_ptr_array_index (priv->array_val, i));
				g_assert (DFSM_IS_AST_EXPRESSION_DATA_STRUCTURE (child_expression));

				/* Get the child variant. */
				child_variant = g_variant_get_child_value (new_value, i);

				/* Recursively assign to the child data structure. */
				dfsm_ast_expression_data_structure_set_from_variant (DFSM_AST_EXPRESSION_DATA_STRUCTURE (child_expression),
				                                                     environment, child_variant);

				g_variant_unref (child_variant);
			}

			break;
		}
		case DFSM_AST_DATA_STRUCT: {
			guint i;

			/* We can safely assume that the struct and the variant have the same type, etc. since that's all been checked before. */
			for (i = 0; i < priv->struct_val->len; i++) {
				GVariant *child_variant;
				DfsmAstExpression *child_expression;

				/* We assume that the child_expression is a DFSM_AST_EXPRESSION_DATA_STRUCTURE and extract its data structure to
				 * recursively assign to. */
				child_expression = DFSM_AST_EXPRESSION (g_ptr_array_index (priv->struct_val, i));
				g_assert (DFSM_IS_AST_EXPRESSION_DATA_STRUCTURE (child_expression));

				/* Get the child variant. */
				child_variant = g_variant_get_child_value (new_value, i);

				/* Recursively assign to the child data structure. */
				dfsm_ast_expression_data_structure_set_from_variant (DFSM_AST_EXPRESSION_DATA_STRUCTURE (child_expression),
				                                                     environment, child_variant);

				g_variant_unref (child_variant);
			}

			break;
		}
		case DFSM_AST_DATA_DICT: {
			GVariantIter iter;
			GVariant *child_entry_variant;
			GHashTable *data_structure_map;
			guint i;

			/* Loop through the data structure and copy all the dict entries into a hash table. This makes lookups faster, ensures that
			 * we're not constantly re-evaluating expressions for keys, and thus means the key values are set in stone before we start
			 * potentially modifying them by making assignments. */
			data_structure_map = g_hash_table_new_full (g_variant_hash, g_variant_equal, (GDestroyNotify) g_variant_unref,
			                                            g_object_unref);

			for (i = 0; i < priv->dict_val->len; i++) {
				DfsmAstDictionaryEntry *child_entry;
				GVariant *key_variant;

				child_entry = (DfsmAstDictionaryEntry*) g_ptr_array_index (priv->dict_val, i);

				/* Evaluate the expression for the entry's key, but not its value. We'll store a pointer to the value verbatim. */
				key_variant = dfsm_ast_expression_evaluate (child_entry->key, environment);

				/* Insert the entry into the map. */
				g_hash_table_insert (data_structure_map, g_variant_ref (key_variant), g_object_ref (child_entry->value));

				g_variant_unref (key_variant);
			}

			/* We should only assign to the values in the data structure dict which are listed in the variant dict. i.e. We touch the
			 * values corresponding to the intersection of the keys of the data structure and variant dicts (plus the difference between
			 * the variant and data structure keys). */
			g_variant_iter_init (&iter, new_value);

			while ((child_entry_variant = g_variant_iter_next_value (&iter)) != NULL) {
				GVariant *child_key_variant, *child_value_variant;
				DfsmAstExpression *value_expression;

				/* Get the child variant and its key and value. */
				child_entry_variant = g_variant_get_child_value (new_value, i);
				child_key_variant = g_variant_get_child_value (child_entry_variant, 0);
				child_value_variant = g_variant_get_child_value (child_entry_variant, 1);

				g_variant_unref (child_entry_variant);

				/* Find the corresponding entry in the data structure dict, if it exists. */
				value_expression = g_hash_table_lookup (data_structure_map, child_key_variant);

				g_variant_unref (child_key_variant);

				/* We assume that the child_expression is a DFSM_AST_EXPRESSION_DATA_STRUCTURE and extract its data structure to
				 * recursively assign to. */
				g_assert (DFSM_IS_AST_EXPRESSION_DATA_STRUCTURE (value_expression));

				/* Recursively assign to the child data structure. */
				dfsm_ast_expression_data_structure_set_from_variant (DFSM_AST_EXPRESSION_DATA_STRUCTURE (value_expression),
				                                                     environment, child_value_variant);

				g_variant_unref (child_value_variant);
			}

			g_hash_table_unref (data_structure_map);

			break;
		}
		case DFSM_AST_DATA_VARIABLE:
			/* Base case. */
			dfsm_ast_variable_set_from_variant (priv->variable_val, environment, new_value);
			break;
		case DFSM_AST_DATA_BYTE:
		case DFSM_AST_DATA_BOOLEAN:
		case DFSM_AST_DATA_INT16:
		case DFSM_AST_DATA_UINT16:
		case DFSM_AST_DATA_INT32:
		case DFSM_AST_DATA_UINT32:
		case DFSM_AST_DATA_INT64:
		case DFSM_AST_DATA_UINT64:
		case DFSM_AST_DATA_DOUBLE:
		case DFSM_AST_DATA_STRING:
		case DFSM_AST_DATA_OBJECT_PATH:
		case DFSM_AST_DATA_SIGNATURE:
		case DFSM_AST_DATA_VARIANT:
		case DFSM_AST_DATA_UNIX_FD:
			/* It's an error to call this function with these data structures. */
		default:
			g_assert_not_reached ();
	}
}

static gboolean
check_expression_is_variable (DfsmAstExpression *expression)
{
	DfsmAstDataStructure *data_structure;

	/* Check that the expression is a DFSM_AST_EXPRESSION_DATA_STRUCTURE. */
	if (DFSM_IS_AST_EXPRESSION_DATA_STRUCTURE (expression) == FALSE) {
		return FALSE;
	}

	/* Recursively check. */
	data_structure = dfsm_ast_expression_data_structure_get_data_structure (DFSM_AST_EXPRESSION_DATA_STRUCTURE (expression));

	return dfsm_ast_data_structure_is_variable (data_structure);
}

/**
 * dfsm_ast_data_structure_is_variable:
 * @self: a #DfsmAstDataStructure
 *
 * Check whether the given #DfsmAstDataStructure has only variables at its leaf nodes (except dictionary keys, which can have any type). This is
 * required for data structures which are assigned to, as assignments can only be made to variables. As such, this function must be called successfully
 * before dfsm_ast_data_structure_set_from_variant() can be called.
 *
 * Return value: %TRUE if the data structure has only variables at its leaves, %FALSE otherwise
 */
gboolean
dfsm_ast_data_structure_is_variable (DfsmAstDataStructure *self)
{
	DfsmAstDataStructurePrivate *priv;

	g_return_val_if_fail (DFSM_IS_AST_DATA_STRUCTURE (self), FALSE);

	priv = self->priv;

	switch (priv->data_structure_type) {
		case DFSM_AST_DATA_ARRAY: {
			guint i;

			for (i = 0; i < priv->array_val->len; i++) {
				if (check_expression_is_variable (DFSM_AST_EXPRESSION (g_ptr_array_index (priv->array_val, i))) == FALSE) {
					return FALSE;
				}
			}

			return TRUE;
		}
		case DFSM_AST_DATA_STRUCT: {
			guint i;

			for (i = 0; i < priv->struct_val->len; i++) {
				if (check_expression_is_variable (DFSM_AST_EXPRESSION (g_ptr_array_index (priv->struct_val, i))) == FALSE) {
					return FALSE;
				}
			}

			return TRUE;
		}
		case DFSM_AST_DATA_DICT: {
			guint i;

			for (i = 0; i < priv->dict_val->len; i++) {
				DfsmAstDictionaryEntry *child_entry;

				/* We check that the value is a variable data structure, but we don't care about the key. It can be anything. */
				child_entry = (DfsmAstDictionaryEntry*) g_ptr_array_index (priv->dict_val, i);

				if (check_expression_is_variable (child_entry->value) == FALSE) {
					return FALSE;
				}
			}

			return TRUE;
		}
		case DFSM_AST_DATA_VARIABLE:
			/* True base case. */
			return TRUE;
		case DFSM_AST_DATA_BYTE:
		case DFSM_AST_DATA_BOOLEAN:
		case DFSM_AST_DATA_INT16:
		case DFSM_AST_DATA_UINT16:
		case DFSM_AST_DATA_INT32:
		case DFSM_AST_DATA_UINT32:
		case DFSM_AST_DATA_INT64:
		case DFSM_AST_DATA_UINT64:
		case DFSM_AST_DATA_DOUBLE:
		case DFSM_AST_DATA_STRING:
		case DFSM_AST_DATA_OBJECT_PATH:
		case DFSM_AST_DATA_SIGNATURE:
		case DFSM_AST_DATA_VARIANT:
		case DFSM_AST_DATA_UNIX_FD:
			/* False base cases. */
			return FALSE;
		default:
			g_assert_not_reached ();
	}
}

/**
 * dfsm_ast_dictionary_entry_new:
 * @key: expression giving entry's key
 * @value: expression giving entry's value
 *
 * Create a new #DfsmAstDictionaryEntry mapping the given @key to the given @value.
 *
 * Return value: (transfer full): a new dictionary entry
 */
DfsmAstDictionaryEntry *
dfsm_ast_dictionary_entry_new (DfsmAstExpression *key, DfsmAstExpression *value)
{
	DfsmAstDictionaryEntry *entry;

	g_return_val_if_fail (DFSM_IS_AST_EXPRESSION (key), NULL);
	g_return_val_if_fail (DFSM_IS_AST_EXPRESSION (value), NULL);

	entry = g_slice_new (DfsmAstDictionaryEntry);

	entry->key = g_object_ref (key);
	entry->value = g_object_ref (value);

	return entry;
}

/**
 * dfsm_ast_dictionary_entry_free:
 * @entry: entry to free
 *
 * Free the given dictionary entry.
 */
void
dfsm_ast_dictionary_entry_free (DfsmAstDictionaryEntry *entry)
{
	if (entry != NULL) {
		g_object_unref (entry->value);
		g_object_unref (entry->key);

		g_slice_free (DfsmAstDictionaryEntry, entry);
	}
}
