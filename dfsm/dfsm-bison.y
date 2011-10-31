%pure-parser
%locations
%defines
%error-verbose

%lex-param {GError **error}
%parse-param {DfsmParserData *parser_data}
%parse-param {GError **error}

%expect 13 /* shift-reduce conflicts */

%code top {
	#include <stdio.h>
	#include <glib.h>
}

%code requires {
	#include "dfsm-ast.h"
	#include "dfsm-parser.h"
}

%code {
	static void yyerror (YYLTYPE *yylloc, DfsmParserData *parser_data, GError **error, const char *message);

	#define YYLEX_PARAM parser_data->yyscanner

	#define ERROR (parser_data->error)
	#define ABORT_ON_ERROR \
	if (ERROR != NULL) { \
		YYABORT; \
	}
}

%union {
	char *str;
	gint64 integer;
	double flt;
	GPtrArray *ptr_array;
	GHashTable *hash_table;
	DfsmAstObject *ast_object;
	DfsmAstDataStructure *ast_data_structure;
	DfsmAstExpression *ast_expression;
	DfsmAstTransition *ast_transition;
	DfsmAstStatement *ast_statement;
	DfsmAstVariable *ast_variable;
	DfsmParserBlockList *block_list;
}

%destructor { free ($$); } <str>
%destructor {} <integer> <flt>
%destructor { if ($$ != NULL) { g_ptr_array_unref ($$); } } <ptr_array>
%destructor { if ($$ != NULL) { g_hash_table_unref ($$); } } <hash_table>
%destructor { dfsm_ast_node_unref ($$); } <ast_object> <ast_data_structure> <ast_expression> <ast_transition> <ast_statement> <ast_variable>
%destructor { dfsm_parser_block_list_free ($$); } <block_list>

/* TODO:
 * Priorities for fuzzing
 */

%token <str> DBUS_OBJECT_PATH
%token <str> DBUS_INTERFACE_NAME
%token <str> DBUS_TYPE_SIGNATURE

%token <str> IDENTIFIER

%token <str> STRING
%token <integer> INTEGER
%token <flt> FLOAT
%token TRUE_LITERAL
%token FALSE_LITERAL
%token <str> REGEXP

%token FLEX_ERROR

%token OBJECT
%token AT
%token IMPLEMENTS
%token THROWING
%token DATA
%token FROM
%token PRECONDITION
%token TO
%token STATES
%token TRANSITION
%token METHOD
%token ON
%token THROW
%token EMIT
%token REPLY
%token L_BRACE
%token R_BRACE
%token L_PAREN
%token R_PAREN
%token ARRAY_L_BRACKET
%token ARRAY_R_BRACKET
%token L_ANGLE
%token R_ANGLE
%token FUZZY
%token DOT

%right NOT
%left TIMES DIVIDE MODULUS
%left PLUS MINUS
%left LT LTE GT GTE
%left EQ NEQ
%left AND
%left OR

%start Input

%type <ptr_array> Input
%type <ast_object> ObjectBlock
%type <block_list> BlockList
%type <ptr_array> InterfaceNameList
%type <hash_table> DataBlock
%type <hash_table> DataList
%type <ptr_array> StatesBlock
%type <ptr_array> StateList
%type <str> StateName
%type <ast_transition> TransitionBlock
%type <str> TransitionType
%type <ptr_array> PreconditionList
%type <str> PreconditionThrow
%type <ptr_array> StatementList
%type <ast_statement> Statement
%type <ast_expression> Expression
%type <str> FunctionName
%type <str> VariableName
%type <ast_variable> Variable
%type <ast_data_structure> FuzzyDataStructure
%type <ast_data_structure> DataStructure
%type <ptr_array> ArrayList
%type <ptr_array> DictionaryList
%type <ptr_array> StructureList StructureListInner

%%

/* Returns a GPtrArray of DfsmAstObjects. */
Input: /* empty */			{ $$ = g_ptr_array_new_with_free_func (dfsm_ast_node_unref); parser_data->object_array = g_ptr_array_ref ($$); }
     | Input ObjectBlock		{ $$ = $1; g_ptr_array_add ($$, $2); }
;

/* Returns a new DfsmAstObject. */
ObjectBlock:
	OBJECT AT DBUS_OBJECT_PATH IMPLEMENTS InterfaceNameList L_BRACE
		BlockList
	R_BRACE									{
											$$ = dfsm_ast_object_new ($3, $5,
											                          g_ptr_array_ref ($7->data_blocks),
											                          g_ptr_array_ref ($7->state_blocks),
											                          g_ptr_array_ref ($7->transitions),
											                          &ERROR);
											ABORT_ON_ERROR;
										}
|	OBJECT AT DBUS_OBJECT_PATH IMPLEMENTS InterfaceNameList L_BRACE
		error
	R_BRACE									{ $$ = NULL; YYABORT; }
;

/* Returns a DfsmParserBlockList */
BlockList: /* empty */								{ $$ = dfsm_parser_block_list_new (); }
         | BlockList DataBlock							{ $$ = $1; g_ptr_array_add ($$->data_blocks, $2); }
         | BlockList StatesBlock						{ $$ = $1; g_ptr_array_add ($$->state_blocks, $2); }
         | BlockList TransitionBlock						{ $$ = $1; g_ptr_array_add ($$->transitions, $2); }
;

/* Returns a GPtrArray of interface names (strings). */
InterfaceNameList: DBUS_INTERFACE_NAME						{
											$$ = g_ptr_array_new_with_free_func (g_free);
											g_ptr_array_add ($$, $1 /* steal */);
										}
                 | InterfaceNameList ',' DBUS_INTERFACE_NAME			{ $$ = $1; g_ptr_array_add ($$, $3); }
;

/* Returns a new GHashTable for the data items. */
DataBlock:
	DATA L_BRACE
		DataList
	R_BRACE									{ $$ = $3; }
|	DATA L_BRACE
		error
	R_BRACE									{ $$ = NULL; YYABORT; }
;

/* Returns a new GHashTable mapping variable names (strings) to DfsmAstDataItems. */
DataList: /* empty */
		{
			$$ = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) dfsm_ast_data_item_free);
		}
        | VariableName ':' DBUS_TYPE_SIGNATURE '=' FuzzyDataStructure
		{
			$$ = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) dfsm_ast_data_item_free);
			g_hash_table_insert ($$, $1 /* steal ownership from flex */, dfsm_ast_data_item_new ($3, $5));
		}
        | VariableName ':' DBUS_TYPE_SIGNATURE '=' FuzzyDataStructure ';' DataList
		{
			$$ = $7;
			g_hash_table_insert ($$, $1 /* steal ownership from flex */, dfsm_ast_data_item_new ($3, $5));
		}
        | error ';' DataList
		{ $$ = $3; YYABORT; }
;

/* Returns a new GPtrArray of the state names. */
StatesBlock:
	STATES L_BRACE
		StateList
	R_BRACE									{ $$ = $3; }
|	STATES L_BRACE
		error
	R_BRACE									{ $$ = NULL; YYABORT; }
;

/* Returns a new GPtrArray of state names (strings). */
StateList: /* empty */								{ $$ = g_ptr_array_new_with_free_func (g_free); }
         | StateName								{
											$$ = g_ptr_array_new_with_free_func (g_free);
											g_ptr_array_add ($$, $1 /* steal */);
										}
         | StateName ';' StateList						{ $$ = $3; g_ptr_array_add ($$, $1 /* steal */); }
         | error ';' StateList							{ $$ = $3; YYABORT; }
;

/* Returns a new string containing the state name. */
StateName: IDENTIFIER								{ $$ = $1; /* steal ownership from flex */ }
;

/* Returns a new DfsmAstTransition. */
TransitionBlock:
	TRANSITION FROM StateName TO StateName ON TransitionType L_BRACE
		PreconditionList
		StatementList
	R_BRACE									{ $$ = dfsm_ast_transition_new ($3, $5, $7, $9, $10, &ERROR);
										  ABORT_ON_ERROR; }
|	TRANSITION FROM StateName TO StateName ON TransitionType L_BRACE
		error
	R_BRACE									{ $$ = NULL; YYABORT; }
;

/* Returns a string representing the transition type. We hackily mix "*" in with method names, since it can never be a valid method name. */
TransitionType: METHOD IDENTIFIER						{ $$ = $2; /* steal ownership from flex */ }
              | '*'								{ $$ = g_strdup ("*"); }
;

/* Returns a new GPtrArray containing DfsmAstPreconditions. */
PreconditionList: /* empty */							{ $$ = g_ptr_array_new_with_free_func (dfsm_ast_node_unref); }
                | PreconditionList PRECONDITION PreconditionThrow L_BRACE Expression R_BRACE
			{
				$$ = $1;
				g_ptr_array_add ($$, dfsm_ast_precondition_new ($3, $5, &ERROR));
				ABORT_ON_ERROR;
			}
                | PreconditionList PRECONDITION PreconditionThrow L_BRACE error R_BRACE
;

/* Returns a string containing the error name, or NULL for no error. */
PreconditionThrow: /* empty */							{ $$ = NULL; }
                 | THROWING IDENTIFIER						{ $$ = $2; /* steal ownership from flex */ }
;

/* Returns a new GPtrArray of DfsmAstStatements. */
StatementList: Statement ';'							{
											$$ = g_ptr_array_new_with_free_func (dfsm_ast_node_unref);
											g_ptr_array_add ($$, $1 /* steal */);
										}
             | StatementList Statement ';'					{ $$ = $1; g_ptr_array_add ($$, $2); }
             | StatementList error ';'						{ $$ = $1; YYABORT; }
;

/* Returns a new DfsmAstStatement (or subclass). */
Statement: DataStructure '=' Expression					{ $$ = dfsm_ast_statement_assignment_new ($1, $3, &ERROR); ABORT_ON_ERROR; }
         | THROW IDENTIFIER						{ $$ = dfsm_ast_statement_throw_new ($2, &ERROR); ABORT_ON_ERROR; }
         | EMIT IDENTIFIER Expression					{ $$ = dfsm_ast_statement_emit_new ($2, $3, &ERROR); ABORT_ON_ERROR; }
         | REPLY Expression						{ $$ = dfsm_ast_statement_reply_new ($2, &ERROR); ABORT_ON_ERROR; }
;

/* Returns a new DfsmAstExpression. */
Expression: L_ANGLE Expression R_ANGLE		{ $$ = $2; }
          | L_ANGLE error R_ANGLE		{ $$ = NULL; YYABORT; }
          | FunctionName Expression		{ $$ = dfsm_ast_expression_function_call_new ($1, $2, &ERROR); ABORT_ON_ERROR; }
          | FuzzyDataStructure			{ $$ = dfsm_ast_expression_data_structure_new ($1, &ERROR); ABORT_ON_ERROR; }
          | NOT Expression			{ $$ = dfsm_ast_expression_unary_new (DFSM_AST_EXPRESSION_NOT, $2, &ERROR); ABORT_ON_ERROR; }
          | Expression TIMES Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_TIMES, $1, $3, &ERROR); ABORT_ON_ERROR; }
          | Expression DIVIDE Expression	{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_DIVIDE, $1, $3, &ERROR); ABORT_ON_ERROR; }
          | Expression MODULUS Expression	{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_MODULUS, $1, $3, &ERROR); ABORT_ON_ERROR; }
          | Expression PLUS Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_PLUS, $1, $3, &ERROR); ABORT_ON_ERROR; }
          | Expression MINUS Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_MINUS, $1, $3, &ERROR); ABORT_ON_ERROR; }
          | Expression LT Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_LT, $1, $3, &ERROR); ABORT_ON_ERROR; }
          | Expression LTE Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_LTE, $1, $3, &ERROR); ABORT_ON_ERROR; }
          | Expression GT Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_GT, $1, $3, &ERROR); ABORT_ON_ERROR; }
          | Expression GTE Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_GTE, $1, $3, &ERROR); ABORT_ON_ERROR; }
          | Expression EQ Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_EQ, $1, $3, &ERROR); ABORT_ON_ERROR; }
          | Expression NEQ Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_NEQ, $1, $3, &ERROR); ABORT_ON_ERROR; }
          | Expression AND Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_AND, $1, $3, &ERROR); ABORT_ON_ERROR; }
          | Expression OR Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_OR, $1, $3, &ERROR); ABORT_ON_ERROR; }
;

/* Returns the function name as a string. */
FunctionName: IDENTIFIER							{ $$ = $1; /* steal ownership from flex */ }
;

/* Returns the variable name as a string. */
VariableName: IDENTIFIER							{ $$ = $1; /* steal ownership from flex */ }
;

/* Returns a new DfsmAstVariable. */
Variable: VariableName								{ $$ = dfsm_ast_variable_new (DFSM_AST_SCOPE_LOCAL,
										                              $1, &ERROR); ABORT_ON_ERROR; }
        | OBJECT DOT VariableName						{ $$ = dfsm_ast_variable_new (DFSM_AST_SCOPE_OBJECT,
										                              $3, &ERROR); ABORT_ON_ERROR; }
;

/* Returns a new DfsmAstDataStructure or DfsmAstFuzzyDataStructure (which is a subclass). */
FuzzyDataStructure: DataStructure						{ $$ = $1; }
                  | DataStructure FUZZY						{ $$ = dfsm_ast_fuzzy_data_structure_new ($1, &ERROR); ABORT_ON_ERROR; }
;

/* Returns a new DfsmAstDataStructure or DfsmAstVariable. */
DataStructure: STRING					{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_STRING, $1, &ERROR); ABORT_ON_ERROR; }
             | INTEGER					{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_INTEGER, &$1, &ERROR); ABORT_ON_ERROR; }
             | FLOAT					{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_FLOAT, &$1, &ERROR); ABORT_ON_ERROR; }
             | TRUE_LITERAL				{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_BOOLEAN,
							                                    GUINT_TO_POINTER (TRUE), &ERROR); ABORT_ON_ERROR; }
             | FALSE_LITERAL				{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_BOOLEAN,
							                                    GUINT_TO_POINTER (FALSE), &ERROR); ABORT_ON_ERROR; }
             | REGEXP					{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_REGEXP, $1, &ERROR); ABORT_ON_ERROR; }
             | ARRAY_L_BRACKET ArrayList ARRAY_R_BRACKET	{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_ARRAY, $2, &ERROR); ABORT_ON_ERROR; }
             | ARRAY_L_BRACKET error ARRAY_R_BRACKET		{ $$ = NULL; YYABORT; }
             | L_BRACE DictionaryList R_BRACE		{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_DICTIONARY, $2, &ERROR); ABORT_ON_ERROR; }
             | L_BRACE error R_BRACE			{ $$ = NULL; YYABORT; }
             | L_PAREN StructureList R_PAREN		{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_STRUCTURE, $2, &ERROR); ABORT_ON_ERROR; }
             | L_PAREN error R_PAREN			{ $$ = NULL; YYABORT; }
             | Variable					{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_VARIABLE, $1, &ERROR); ABORT_ON_ERROR; }
;

/* Returns a new GPtrArray of DfsmAstExpressions */
ArrayList: /* empty */								{ $$ = g_ptr_array_new_with_free_func (dfsm_ast_node_unref); }
         | ArrayList Expression ','						{ $$ = $1; g_ptr_array_add ($$, $2); }
;

/* Returns a new GPtrArray of DfsmAstDictionaryEntrys */
DictionaryList: /* empty */				{
								$$ = g_ptr_array_new_with_free_func ((GDestroyNotify) dfsm_ast_dictionary_entry_free);
							}
              | DictionaryList Expression ':' Expression ','			{
											$$ = $1;
											g_ptr_array_add ($$, dfsm_ast_dictionary_entry_new ($2, $4));
										}
;

/* Returns a new GPtrArray of DfsmAstExpressions */
StructureList: StructureListInner						{ $$ = $1; }
             | StructureListInner Expression					{ $$ = $1; g_ptr_array_add ($$, $2); }
;
StructureListInner: /* empty */							{ $$ = g_ptr_array_new_with_free_func (dfsm_ast_node_unref); }
                  | StructureListInner Expression ','				{ $$ = $1; g_ptr_array_add ($$, $2); }
;

%%

GPtrArray *
dfsm_bison_parse (const gchar *source_buf, GError **error)
{
	GError *child_error = NULL;
	DfsmParserData data = { 0, };
	int result;
	GPtrArray *retval = NULL;

	/* Set up the parsing environment. */
	data.yyscanner = NULL;
	data.object_array = NULL;
	data.error = child_error;

	data.source_buf = source_buf;
	data.source_len = g_utf8_strlen (source_buf, -1);
	data.source_pos = 0;

	yylex_init (&(data.yyscanner));
	yyset_extra (&data, data.yyscanner);

	/* Parse! */
	result = yyparse (&data, &child_error);

	yylex_destroy (data.yyscanner);

	if (result == 0) {
		/* Success! */
		g_assert (data.object_array != NULL);
		retval = g_ptr_array_ref (data.object_array);
	} else if (result == 1) {
		/* Parse error */
		g_assert (child_error != NULL);
		g_propagate_error (error, child_error);
		retval = NULL;
	} else if (result == 2) {
		/* OOM */
		retval = NULL;

		/* If an error had already been set, return that; otherwise report the OOM. */
		if (child_error != NULL) {
			g_propagate_error (error, child_error);
		} else {
			/* TODO */
		}
	} else {
		g_assert_not_reached ();
	}

	/* Tidy up */
	g_clear_error (&child_error);
	if (data.object_array != NULL) {
		g_ptr_array_unref (data.object_array);
	}

	g_assert ((retval != NULL && child_error == NULL) || (retval == NULL && child_error != NULL));

	return retval;
}

static void
yyerror (YYLTYPE *yylloc, DfsmParserData *parser_data, GError **error, const char *message)
{
	fprintf (stderr, "%s\n", message);
}
