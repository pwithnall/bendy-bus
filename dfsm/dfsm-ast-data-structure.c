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

#include <glib.h>

#include "dfsm-ast-data-structure.h"
#include "dfsm-ast-expression-data-structure.h"
#include "dfsm-ast-variable.h"
#include "dfsm-parser.h"

static void dfsm_ast_data_structure_finalize (GObject *object);
static void dfsm_ast_data_structure_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);

struct _DfsmAstDataStructurePrivate {
	DfsmAstDataStructureType data_structure_type;
	gdouble weight;
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
		GVariant *variant_val;
		GPtrArray/*<DfsmAstDictionaryEntry>*/ *dict_val;
		gint unix_fd_val;
		gchar *regexp_val;
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
			g_variant_unref (priv->variant_val);
			break;
		case DFSM_AST_DATA_DICT:
			g_ptr_array_unref (priv->dict_val);
			break;
		case DFSM_AST_DATA_UNIX_FD:
			/* Nothing to free here */
			break;
		case DFSM_AST_DATA_REGEXP:
			g_free (priv->regexp_val);
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
dfsm_ast_data_structure_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstDataStructurePrivate *priv = DFSM_AST_DATA_STRUCTURE (node)->priv;
	guint i;

	/* Conditions which should always hold, regardless of user input. */
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
			}

			break;
		case DFSM_AST_DATA_STRUCT:
			g_assert (priv->struct_val != NULL);

			for (i = 0; i < priv->struct_val->len; i++) {
				g_assert (g_ptr_array_index (priv->struct_val, i) != NULL);
			}

			break;
		case DFSM_AST_DATA_VARIANT:
			g_assert (priv->variant_val != NULL);
			break;
		case DFSM_AST_DATA_DICT:
			g_assert (priv->dict_val != NULL);

			for (i = 0; i < priv->dict_val->len; i++) {
				g_assert (g_ptr_array_index (priv->dict_val, i) != NULL);
			}

			break;
		case DFSM_AST_DATA_UNIX_FD:
			/* Nothing to do here */
			break;
		case DFSM_AST_DATA_REGEXP:
			g_assert (priv->regexp_val != NULL);
			break;
		case DFSM_AST_DATA_VARIABLE:
			g_assert (priv->variable_val != NULL);
			break;
		default:
			g_assert_not_reached ();
	}

	/* Conditions which may not hold as a result of invalid user input. */
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
			/* Valid UTF-8? */
			if (g_utf8_validate (priv->string_val, -1, NULL) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             "Invalid UTF-8 in string: %s", priv->string_val);
				return;
			}

			break;
		case DFSM_AST_DATA_OBJECT_PATH:
			/* Valid object path? */
			if (g_variant_is_object_path (priv->object_path_val) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             "Invalid D-Bus object path: %s", priv->object_path_val);
				return;
			}

			break;
		case DFSM_AST_DATA_SIGNATURE:
			/* Valid signature? */
			if (g_variant_type_string_is_valid ((gchar*) priv->signature_val) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             "Invalid D-Bus type signature: %s", priv->signature_val);
				return;
			}

			break;
		case DFSM_AST_DATA_ARRAY: {
			GVariantType *expected_type = NULL;

			/* All entries valid and of the same type? */
			for (i = 0; i < priv->array_val->len; i++) {
				GVariantType *child_type;
				DfsmAstExpression *expr;

				expr = g_ptr_array_index (priv->array_val, i);

				/* Valid expression? */
				dfsm_ast_node_check ((DfsmAstNode*) expr, environment, error);

				if (*error != NULL) {
					goto array_error;
				}

				/* Equal type? */
				child_type = dfsm_ast_expression_calculate_type (expr, environment);

				if (expected_type == NULL) {
					/* First iteration */
					expected_type = g_variant_type_copy (child_type);
				} else if (g_variant_type_equal (expected_type, child_type) == FALSE) {
					gchar *expected, *received;

					expected = g_variant_type_dup_string (expected_type);
					received = g_variant_type_dup_string (child_type);

					g_variant_type_free (child_type);

					g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
					             "Type mismatch between elements in array: expected type %s but received type %s.",
					             expected, received);

					goto array_error;
				}

				g_variant_type_free (child_type);
			}

		array_error:
			g_variant_type_free (expected_type);

			break;
		}
		case DFSM_AST_DATA_STRUCT: {
			/* All entries valid? */
			for (i = 0; i < priv->struct_val->len; i++) {
				DfsmAstExpression *expr;

				expr = g_ptr_array_index (priv->struct_val, i);

				dfsm_ast_node_check ((DfsmAstNode*) expr, environment, error);

				if (*error != NULL) {
					return;
				}
			}

			break;
		}
		case DFSM_AST_DATA_VARIANT:
			if (g_variant_is_normal_form (priv->variant_val) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid variant: not in normal form");
				return;
			}

			break;
		case DFSM_AST_DATA_DICT: {
			GVariantType *expected_key_type = NULL, *expected_value_type = NULL;

			/* All entries valid with no duplicate keys and equal types for all keys and values? */
			for (i = 0; i < priv->dict_val->len; i++) {
				GVariantType *key_type, *value_type;
				DfsmAstDictionaryEntry *entry;

				/* Valid expressions? */
				entry = g_ptr_array_index (priv->dict_val, i);

				dfsm_ast_node_check ((DfsmAstNode*) entry->key, environment, error);

				if (*error != NULL) {
					goto dict_error;
				}

				dfsm_ast_node_check ((DfsmAstNode*) entry->value, environment, error);

				if (*error != NULL) {
					goto dict_error;
				}

				/* Equal types? */
				key_type = dfsm_ast_expression_calculate_type (entry->key, environment);
				value_type = dfsm_ast_expression_calculate_type (entry->value, environment);

				g_assert ((expected_key_type == NULL) == (expected_value_type == NULL));

				if (expected_key_type == NULL) {
					/* First iteration */
					expected_key_type = g_variant_type_copy (key_type);
					expected_value_type = g_variant_type_copy (value_type);
				} else if (g_variant_type_equal (expected_key_type, key_type) == FALSE) {
					gchar *expected, *received;

					expected = g_variant_type_dup_string (expected_key_type);
					received = g_variant_type_dup_string (key_type);

					g_variant_type_free (key_type);
					g_variant_type_free (value_type);

					g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
					             "Type mismatch between keys in dictionary: expected type %s but received type %s.",
					             expected, received);

					goto dict_error;
				} else if (g_variant_type_equal (expected_value_type, value_type) == FALSE) {
					gchar *expected, *received;

					expected = g_variant_type_dup_string (expected_value_type);
					received = g_variant_type_dup_string (value_type);

					g_variant_type_free (key_type);
					g_variant_type_free (value_type);

					g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
					             "Type mismatch between values in dictionary: expected type %s but received type %s.",
					             expected, received);

					goto dict_error;
				}

				g_variant_type_free (key_type);
				g_variant_type_free (value_type);
			}

		dict_error:
			g_variant_type_free (expected_key_type);
			g_variant_type_free (expected_value_type);

			break;
		}
		case DFSM_AST_DATA_UNIX_FD:
			/* Nothing to do here */
			break;
		case DFSM_AST_DATA_REGEXP: {
			/* Check if the regexp is valid by trying to parse it */
			GRegex *regex = g_regex_new (priv->regexp_val, 0, 0, error);

			if (regex != NULL) {
				g_regex_unref (regex);
			}

			if (*error != NULL) {
				return;
			}

			break;
		}
		case DFSM_AST_DATA_VARIABLE:
			/* Valid variable? */
			dfsm_ast_node_check ((DfsmAstNode*) priv->variable_val, environment, error);

			if (*error != NULL) {
				return;
			}

			break;
		default:
			g_assert_not_reached ();
	}
}

/**
 * dfsm_ast_data_structure_new:
 * @data_structure_type: the type of the outermost data structure
 * @value: value of the data structure
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstDataStructure of type @data_structure_type and containing the given @value.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstDataStructure *
dfsm_ast_data_structure_new (DfsmAstDataStructureType data_structure_type, gpointer value, GError **error)
{
	DfsmAstDataStructure *data_structure;
	DfsmAstDataStructurePrivate *priv;

	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	data_structure = g_object_new (DFSM_TYPE_AST_DATA_STRUCTURE, NULL);
	priv = data_structure->priv;

	switch (data_structure_type) {
		case DFSM_AST_DATA_BYTE:
			priv->byte_val = *((guint64*) value);
			break;
		case DFSM_AST_DATA_BOOLEAN:
			priv->boolean_val = (GPOINTER_TO_UINT (value) == 1) ? TRUE : FALSE;
			break;
		case DFSM_AST_DATA_INT16:
			priv->int16_val = *((gint64*) value);
			break;
		case DFSM_AST_DATA_UINT16:
			priv->uint16_val = *((guint64*) value);
			break;
		case DFSM_AST_DATA_INT32:
			priv->int32_val = *((gint64*) value);
			break;
		case DFSM_AST_DATA_UINT32:
			priv->uint32_val = *((guint64*) value);
			break;
		case DFSM_AST_DATA_INT64:
			priv->int64_val = *((gint64*) value);
			break;
		case DFSM_AST_DATA_UINT64:
			priv->uint64_val = *((guint64*) value);
			break;
		case DFSM_AST_DATA_DOUBLE:
			priv->double_val = *((gdouble*) value);
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
			/* Note: not representable in the FSM language. */
			priv->variant_val = NULL;
			break;
		case DFSM_AST_DATA_DICT:
			priv->dict_val = g_ptr_array_ref (value); /* array of DfsmAstDictionaryEntrys */
			break;
		case DFSM_AST_DATA_UNIX_FD:
			/* Note: not representable in the FSM language. */
			priv->unix_fd_val = 0;
			break;
		case DFSM_AST_DATA_REGEXP:
			priv->regexp_val = g_strdup ((gchar*) value);
			break;
		case DFSM_AST_DATA_VARIABLE:
			priv->variable_val = g_object_ref (value); /* DfsmAstVariable */
			break;
		default:
			g_assert_not_reached ();
	}

	priv->data_structure_type = data_structure_type;
	priv->weight = -1.0;

	return data_structure;
}

/**
 * dfsm_ast_data_structure_set_weight:
 * @self: a #DfsmAstDataStructure
 * @weight: weight of the structure for fuzzing, or %NAN
 *
 * Set the weight of the data structure. The @weight determines how “important” the datastructure is for fuzzing. %NAN indicates no preference as to
 * the structure's weight, and negative values mean the structure shouldn't be fuzzed.
 */
void
dfsm_ast_data_structure_set_weight (DfsmAstDataStructure *self, gdouble weight)
{
	g_return_if_fail (DFSM_IS_AST_DATA_STRUCTURE (self));

	self->priv->weight = weight;
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
	DfsmAstDataStructurePrivate *priv;

	g_return_val_if_fail (DFSM_IS_AST_DATA_STRUCTURE (self), NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);

	priv = self->priv;

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
			GVariantType *array_type, *child_type;

			/* Empty arrays need special-casing. */
			if (priv->array_val->len == 0) {
				return g_variant_type_copy (G_VARIANT_TYPE_ARRAY);
			}

			/* Having checked the array already, we can assume all elements are of the same type. */
			child_type = dfsm_ast_expression_calculate_type (g_ptr_array_index (priv->array_val, 0), environment);
			array_type = g_variant_type_new_array (child_type);
			g_variant_type_free (child_type);

			return array_type;
		}
		case DFSM_AST_DATA_STRUCT: {
			GVariantType *struct_type;
			GPtrArray/*<GVariantType>*/ *child_types;
			guint i;

			/* Empty structs need special-casing. */
			if (priv->struct_val->len == 0) {
				return g_variant_type_copy (G_VARIANT_TYPE_TUPLE);
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
			DfsmAstDictionaryEntry *entry;
			GVariantType *dict_type, *entry_type, *key_type, *value_type;

			/* Empty dictionaries need special-casing. */
			if (priv->dict_val->len == 0) {
				return g_variant_type_copy (G_VARIANT_TYPE_DICTIONARY);
			}

			/* Otherwise, we assume that all entries have the same type (since we checked this before). */
			entry = (DfsmAstDictionaryEntry*) g_ptr_array_index (priv->dict_val, 0);

			key_type = dfsm_ast_expression_calculate_type (entry->key, environment);
			value_type = dfsm_ast_expression_calculate_type (entry->value, environment);
			entry_type = g_variant_type_new_dict_entry (key_type, value_type);
			g_variant_type_free (value_type);
			g_variant_type_free (key_type);

			dict_type = g_variant_type_new_array (entry_type);

			g_variant_type_free (entry_type);

			return dict_type;
		}
		case DFSM_AST_DATA_UNIX_FD:
			return g_variant_type_copy (G_VARIANT_TYPE_UINT32);
		case DFSM_AST_DATA_REGEXP:
			return g_variant_type_copy (G_VARIANT_TYPE_STRING);
		case DFSM_AST_DATA_VARIABLE:
			return dfsm_ast_variable_calculate_type (priv->variable_val, environment);
		default:
			g_assert_not_reached ();
	}
}

/**
 * dfsm_ast_data_structure_to_variant:
 * @self: a #DfsmAstDataStructure
 * @environment: a #DfsmEnvironment containing all variables
 * @error: (allow-none): a #GError, or %NULL
 *
 * Convert the data structure given by @self to a #GVariant in the given @environment.
 *
 * This assumes that the data structure has been successfully checked by dfsm_ast_node_check() beforehand. It is an error to call this function
 * otherwise.
 *
 * Return value: (transfer full): the non-floating #GVariant representation of the data structure
 */
GVariant *
dfsm_ast_data_structure_to_variant (DfsmAstDataStructure *self, DfsmEnvironment *environment, GError **error)
{
	DfsmAstDataStructurePrivate *priv;

	g_return_val_if_fail (DFSM_IS_AST_DATA_STRUCTURE (self), NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	priv = self->priv;

	/* NOTE: We have to sink all floating references from here to guarantee that we always return a value of the same floatiness. The alternative
	 * is to always return a floating reference, but that would require modifying dfsm_ast_variable_to_variant() to somehow return a floating
	 * reference, which in turn probably means deep-copying the variable value. Ouch. */

	switch (priv->data_structure_type) {
		case DFSM_AST_DATA_BYTE:
			return g_variant_ref_sink (g_variant_new_byte (priv->byte_val));
		case DFSM_AST_DATA_BOOLEAN:
			return g_variant_ref_sink (g_variant_new_boolean (priv->boolean_val));
		case DFSM_AST_DATA_INT16:
			return g_variant_ref_sink (g_variant_new_int16 (priv->int16_val));
		case DFSM_AST_DATA_UINT16:
			return g_variant_ref_sink (g_variant_new_uint16 (priv->uint16_val));
		case DFSM_AST_DATA_INT32:
			return g_variant_ref_sink (g_variant_new_int32 (priv->int32_val));
		case DFSM_AST_DATA_UINT32:
			return g_variant_ref_sink (g_variant_new_uint32 (priv->uint32_val));
		case DFSM_AST_DATA_INT64:
			return g_variant_ref_sink (g_variant_new_int64 (priv->int64_val));
		case DFSM_AST_DATA_UINT64:
			return g_variant_ref_sink (g_variant_new_uint64 (priv->uint64_val));
		case DFSM_AST_DATA_DOUBLE:
			return g_variant_ref_sink (g_variant_new_double (priv->double_val));
		case DFSM_AST_DATA_STRING:
			return g_variant_ref_sink (g_variant_new_string (priv->string_val));
		case DFSM_AST_DATA_OBJECT_PATH:
			return g_variant_ref_sink (g_variant_new_object_path (priv->object_path_val));
		case DFSM_AST_DATA_SIGNATURE:
			return g_variant_ref_sink (g_variant_new_signature (priv->signature_val));
		case DFSM_AST_DATA_ARRAY: {
			GVariantBuilder builder;
			guint i;

			g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

			for (i = 0; i < priv->array_val->len; i++) {
				GVariant *child_value;
				DfsmAstExpression *child_expression;
				GError *child_error = NULL;

				/* Evaluate the child expression to get a GVariant value. */
				child_expression = (DfsmAstExpression*) g_ptr_array_index (priv->array_val, i);
				child_value = dfsm_ast_expression_evaluate (child_expression, environment, &child_error);

				if (child_error != NULL) {
					/* Error! */
					g_propagate_error (error, child_error);
					return NULL;
				}

				/* Add it to the growing GVariant array. */
				g_variant_builder_add_value (&builder, child_value);

				g_variant_unref (child_value);
			}

			return g_variant_ref_sink (g_variant_builder_end (&builder));
		}
		case DFSM_AST_DATA_STRUCT: {
			GVariantBuilder builder;
			guint i;

			g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);

			for (i = 0; i < priv->struct_val->len; i++) {
				GVariant *child_value;
				DfsmAstExpression *child_expression;
				GError *child_error = NULL;

				/* Evaluate the child expression to get a GVariant value. */
				child_expression = (DfsmAstExpression*) g_ptr_array_index (priv->struct_val, i);
				child_value = dfsm_ast_expression_evaluate (child_expression, environment, &child_error);

				if (child_error != NULL) {
					/* Error! */
					g_propagate_error (error, child_error);
					return NULL;
				}

				/* Add it to the growing GVariant struct. */
				g_variant_builder_add_value (&builder, child_value);

				g_variant_unref (child_value);
			}

			return g_variant_ref_sink (g_variant_builder_end (&builder));
		}
		case DFSM_AST_DATA_VARIANT:
			return g_variant_ref_sink (g_variant_new_variant (priv->variant_val));
		case DFSM_AST_DATA_DICT: {
			GVariantBuilder builder;
			guint i;

			g_variant_builder_init (&builder, G_VARIANT_TYPE_DICTIONARY);

			for (i = 0; i < priv->dict_val->len; i++) {
				GVariant *key_value, *value_value;
				DfsmAstDictionaryEntry *dict_entry;
				GError *child_error = NULL;

				/* Evaluate the child expressions to get GVariant values. */
				dict_entry = (DfsmAstDictionaryEntry*) g_ptr_array_index (priv->dict_val, i);

				key_value = dfsm_ast_expression_evaluate (dict_entry->key, environment, &child_error);

				if (child_error != NULL) {
					/* Error! */
					g_propagate_error (error, child_error);
					return NULL;
				}

				value_value = dfsm_ast_expression_evaluate (dict_entry->value, environment, &child_error);

				if (child_error != NULL) {
					/* Error! */
					g_variant_unref (key_value);
					g_propagate_error (error, child_error);
					return NULL;
				}

				/* Add them to the growing GVariant dict. */
				g_variant_builder_open (&builder, G_VARIANT_TYPE_DICT_ENTRY);
				g_variant_builder_add_value (&builder, key_value);
				g_variant_builder_add_value (&builder, value_value);
				g_variant_builder_close (&builder);

				g_variant_unref (value_value);
				g_variant_unref (key_value);
			}

			return g_variant_ref_sink (g_variant_builder_end (&builder));
		}
		case DFSM_AST_DATA_UNIX_FD:
			return g_variant_ref_sink (g_variant_new_uint32 (priv->unix_fd_val));
		case DFSM_AST_DATA_REGEXP:
			return g_variant_ref_sink (g_variant_new_string (priv->regexp_val));
		case DFSM_AST_DATA_VARIABLE:
			return dfsm_ast_variable_to_variant (priv->variable_val, environment, error);
		default:
			g_assert_not_reached ();
	}
}

/**
 * dfsm_ast_data_structure_set_from_variant:
 * @self: a #DfsmAstDataStructure
 * @environment: a #DfsmEnvironment containing all variables
 * @new_value: the #GVariant value to assign to @self
 * @error: (allow-none): a #GError, or %NULL
 *
 * Set the given @self's value in @environment to the #GVariant value given in @new_value. This will recursively assign to child data
 * structures inside the data structure (e.g. if the data structure is an array of variables, each of the variables will be assigned to).
 *
 * It's an error to call this function with a data structure which isn't comprised entirely of variables or structures of them. Similarly, it's an
 * error to call this function with a @new_value which doesn't match the data structure's type and number of elements.
 */
void
dfsm_ast_data_structure_set_from_variant (DfsmAstDataStructure *self, DfsmEnvironment *environment, GVariant *new_value, GError **error)
{
	DfsmAstDataStructurePrivate *priv;

	g_return_if_fail (DFSM_IS_AST_DATA_STRUCTURE (self));
	g_return_if_fail (DFSM_IS_ENVIRONMENT (environment));
	g_return_if_fail (new_value != NULL);
	g_return_if_fail (error == NULL || *error == NULL);

	priv = self->priv;

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
		case DFSM_AST_DATA_VARIANT:
		case DFSM_AST_DATA_UNIX_FD:
		case DFSM_AST_DATA_REGEXP:
			g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid assignment to a basic data structure.");
			break;
		case DFSM_AST_DATA_ARRAY: {
			guint i;

			/* We can safely assume that the array and the variant have the same type, length, etc. since that's all been checked
			 * before. */
			for (i = 0; i < priv->array_val->len; i++) {
				GVariant *child_variant;
				DfsmAstExpression *child_expression;
				GError *child_error = NULL;

				/* TODO: For the moment, we hackily assume that the child_expression is a DFSM_AST_EXPRESSION_DATA_STRUCTURE and
				 * extract its data structure to recursively assign to. */
				child_expression = (DfsmAstExpression*) g_ptr_array_index (priv->array_val, i);
				g_assert (DFSM_IS_AST_EXPRESSION_DATA_STRUCTURE (child_expression));

				/* Get the child variant. */
				child_variant = g_variant_get_child_value (new_value, i);

				/* Recursively assign to the child data structure. */
				dfsm_ast_expression_data_structure_set_from_variant (DFSM_AST_EXPRESSION_DATA_STRUCTURE (child_expression),
				                                                     environment, child_variant, &child_error);

				g_variant_unref (child_variant);

				if (child_error != NULL) {
					/* Error! */
					g_propagate_error (error, child_error);
					return;
				}
			}

			break;
		}
		case DFSM_AST_DATA_STRUCT: {
			guint i;

			/* We can safely assume that the struct and the variant have the same type, etc. since that's all been checked before. */
			for (i = 0; i < priv->struct_val->len; i++) {
				GVariant *child_variant;
				DfsmAstExpression *child_expression;
				GError *child_error = NULL;

				/* TODO: For the moment, we hackily assume that the child_expression is a DFSM_AST_EXPRESSION_DATA_STRUCTURE and
				 * extract its data structure to recursively assign to. */
				child_expression = (DfsmAstExpression*) g_ptr_array_index (priv->struct_val, i);
				g_assert (DFSM_IS_AST_EXPRESSION_DATA_STRUCTURE (child_expression));

				/* Get the child variant. */
				child_variant = g_variant_get_child_value (new_value, i);

				/* Recursively assign to the child data structure. */
				dfsm_ast_expression_data_structure_set_from_variant (DFSM_AST_EXPRESSION_DATA_STRUCTURE (child_expression),
				                                                     environment, child_variant, &child_error);

				g_variant_unref (child_variant);

				if (child_error != NULL) {
					/* Error! */
					g_propagate_error (error, child_error);
					return;
				}
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
				GError *child_error = NULL;

				child_entry = (DfsmAstDictionaryEntry*) g_ptr_array_index (priv->dict_val, i);

				/* Evaluate the expression for the entry's key, but not its value. We'll store a pointer to the value verbatim. */
				key_variant = dfsm_ast_expression_evaluate (child_entry->key, environment, &child_error);

				if (child_error != NULL) {
					g_hash_table_unref (data_structure_map);
					g_propagate_error (error, child_error);
					return;
				}

				/* Insert the entry into the map. */
				g_hash_table_insert (data_structure_map, g_variant_ref (key_variant), g_object_ref (child_entry->value));

				g_variant_unref (key_variant);
			}

			/* We should only assign to the values in the data structure dict which are listed in the variant dict. i.e. We touch the
			 * values corresponding to the intersection of the keys of the data structure and variant dicts. */
			g_variant_iter_init (&iter, new_value);

			while ((child_entry_variant = g_variant_iter_next_value (&iter)) != NULL) {
				GVariant *child_key_variant, *child_value_variant;
				DfsmAstExpression *value_expression;
				GError *child_error = NULL;

				/* Get the child variant and its key and value. */
				child_entry_variant = g_variant_get_child_value (new_value, i);
				child_key_variant = g_variant_get_child_value (child_entry_variant, 0);
				child_value_variant = g_variant_get_child_value (child_entry_variant, 1);

				g_variant_unref (child_entry_variant);

				/* Find the corresponding entry in the data structure dict, if it exists. */
				value_expression = g_hash_table_lookup (data_structure_map, child_key_variant);

				g_variant_unref (child_key_variant);

				/* TODO: For the moment, we hackily assume that the child_expression is a DFSM_AST_EXPRESSION_DATA_STRUCTURE and
				 * extract its data structure to recursively assign to. */
				g_assert (DFSM_IS_AST_EXPRESSION_DATA_STRUCTURE (value_expression));

				/* Recursively assign to the child data structure. */
				dfsm_ast_expression_data_structure_set_from_variant (DFSM_AST_EXPRESSION_DATA_STRUCTURE (value_expression),
				                                                     environment, child_value_variant, &child_error);

				g_variant_unref (child_value_variant);

				if (child_error != NULL) {
					/* Error! */
					g_hash_table_unref (data_structure_map);
					g_propagate_error (error, child_error);
					return;
				}
			}

			g_hash_table_unref (data_structure_map);

			break;
		}
		case DFSM_AST_DATA_VARIABLE:
			dfsm_ast_variable_set_from_variant (priv->variable_val, environment, new_value, error);
			break;
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