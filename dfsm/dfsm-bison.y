%pure-parser
%locations
%defines
%error-verbose

%lex-param {GError **error}
%parse-param {DfsmParserData *parser_data}
%parse-param {GError **error}

%expect 0 /* shift-reduce conflicts */
%expect-rr 0 /* reduce-reduce conflicts */

%code top {
	#include "config.h"

	#include <stdio.h>
	#include <math.h>
	#include <glib.h>
	#include <glib/gi18n-lib.h>
}

%code requires {
	#include "dfsm-ast.h"
	#include "dfsm-parser.h"
	#include "dfsm-parser-internal.h"
}

%code {
	/* We can't use flex's nice --header-file option because ylwrap sucks and doesn't copy it out.
	 * Consequently, we have to declare the various lexer functions manually. */
	extern int yylex_init (void **scanner);
	extern int yylex_destroy (void *yyscanner);
	extern int yylex (YYSTYPE *yylval_param, YYLTYPE *yylloc_param, void *yyscanner);
	extern void yyset_extra (DfsmParserData *user_defined, void *yyscanner);

	static void yyerror (YYLTYPE *yylloc, DfsmParserData *parser_data, GError **error, const char *message);

	#define YYLEX_PARAM parser_data->yyscanner
}

%union {
	gchar *str;
	gint64 integer;
	guint64 unsigned_integer;
	gdouble flt;
	GPtrArray *ptr_array;
	GHashTable *hash_table;
	DfsmAstObject *ast_object;
	DfsmAstDataStructure *ast_data_structure;
	DfsmAstExpression *ast_expression;
	DfsmAstTransition *ast_transition;
	DfsmAstPrecondition *ast_precondition;
	DfsmAstStatement *ast_statement;
	DfsmAstVariable *ast_variable;
	DfsmParserBlockList *block_list;
	DfsmParserTransitionDetails *transition_details;
	DfsmParserTransitionBlock *transition_block;
	DfsmParserStatePair *state_pair;
}

%destructor { free ($$); } <str>
%destructor {} <integer> <unsigned_integer> <flt>
%destructor { if ($$ != NULL) { g_ptr_array_unref ($$); } } <ptr_array>
%destructor { if ($$ != NULL) { g_hash_table_unref ($$); } } <hash_table>
%destructor { g_object_unref ($$); } <ast_object> <ast_data_structure> <ast_expression> <ast_transition>
	<ast_precondition> <ast_statement> <ast_variable>
%destructor { dfsm_parser_block_list_free ($$); } <block_list>
%destructor { dfsm_parser_transition_details_free ($$); } <transition_details>
%destructor { dfsm_parser_transition_block_free ($$); } <transition_block>
%destructor { dfsm_parser_state_pair_free ($$); } <state_pair>

%token <str> DBUS_OBJECT_PATH
%token <str> DBUS_NAME

%token <str> IDENTIFIER
%token <str> TYPE_ANNOTATION

%token <str> STRING
%token <unsigned_integer> BYTE
%token <integer> INT16
%token <unsigned_integer> UINT16
%token <integer> INT32
%token <unsigned_integer> UINT32
%token <integer> INT64
%token <unsigned_integer> UINT64
%token <flt> DOUBLE
%token TRUE_LITERAL
%token FALSE_LITERAL

%token FLEX_ERROR

%token OBJECT
%token AT
%token IMPLEMENTS
%token THROWING
%token DATA
%token FROM
%token PRECONDITION
%token TO
%token INSIDE
%token STATES
%token TRANSITION
%token METHOD
%token PROPERTY
%token RANDOM
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
%token FUNC /* not actually a real token; just for expression precedence */

%left OR
%left AND
%left EQ NEQ
%left LT LTE GT GTE
%left PLUS MINUS
%left TIMES DIVIDE MODULUS
%right NOT
%right FUNC

%start Input

%type <ptr_array> ObjectBlockList
%type <ast_object> ObjectBlock
%type <ptr_array> BusNameList
%type <str> DBusWellKnownBusName
%type <block_list> BlockList
%type <ptr_array> InterfaceNameList
%type <str> DBusInterfaceName
%type <hash_table> DataBlock
%type <hash_table> DataList
%type <ptr_array> StatesBlock
%type <ptr_array> StateList
%type <str> StateName
%type <transition_block> TransitionBlock
%type <ptr_array> StatePairList
%type <state_pair> StatePair
%type <str> DBusMethodName
%type <str> DBusPropertyName
%type <transition_details> TransitionType
%type <ptr_array> PreconditionList
%type <ast_precondition> Precondition
%type <str> DBusErrorName
%type <str> PreconditionThrow
%type <ptr_array> StatementList
%type <str> DBusSignalName
%type <ast_statement> Statement
%type <ast_expression> Expression
%type <str> FunctionName
%type <str> VariableName
%type <ast_variable> Variable
%type <ast_data_structure> FuzzyDataStructure
%type <ast_data_structure> AnnotatedDataStructure
%type <ast_data_structure> DataStructure
%type <ptr_array> ArrayList ArrayListInner
%type <ptr_array> DictionaryList DictionaryListInner
%type <ptr_array> StructureList StructureListInner

%%

/* Returns nothing. */
Input: ObjectBlockList			{ parser_data->object_array = $1 /* steal */; }
;

/* Returns a GPtrArray of DfsmAstObjects. */
ObjectBlockList: /* empty */			{ $$ = g_ptr_array_new_with_free_func (g_object_unref); }
               | ObjectBlockList ObjectBlock	{ $$ = $1; g_ptr_array_add ($$, $2 /* steal */); }
               | error ObjectBlock		{ $$ = NULL; g_object_unref ($2); YYABORT; }
;

/* Returns a new DfsmAstObject. */
ObjectBlock:
	OBJECT AT DBUS_OBJECT_PATH BusNameList IMPLEMENTS InterfaceNameList L_BRACE
		BlockList
	R_BRACE									{
											$$ = dfsm_ast_object_new (parser_data->dbus_node_info, $3, $4,
											                          $6, $8->data_blocks, $8->state_blocks,
											                          $8->transitions);
											g_free ($3); g_ptr_array_unref ($4); g_ptr_array_unref ($6);
											dfsm_parser_block_list_free ($8);
										}
|	OBJECT AT DBUS_OBJECT_PATH BusNameList IMPLEMENTS InterfaceNameList L_BRACE
		error
	R_BRACE									{ $$ = NULL;
										  g_free ($3); g_ptr_array_unref ($4); g_ptr_array_unref ($6);
										  YYABORT; }
;

/* Returns a new GPtrArray<string>. */
BusNameList: /* empty */							{ $$ = g_ptr_array_new_with_free_func (g_free); }
           | BusNameList ',' DBusWellKnownBusName				{ $$ = $1; g_ptr_array_add ($$, $3 /* steal */); }
;

/* Returns a new string containing the well-known bus name. */
DBusWellKnownBusName: DBUS_NAME							{ $$ = $1; /* steal ownership from flex */ }
;

/* Returns a DfsmParserBlockList */
BlockList: /* empty */								{ $$ = dfsm_parser_block_list_new (); }
         | BlockList DataBlock							{ $$ = $1; g_ptr_array_add ($$->data_blocks, $2 /* steal */); }
         | BlockList StatesBlock						{ $$ = $1; g_ptr_array_add ($$->state_blocks, $2 /* steal */); }
         | BlockList TransitionBlock						{ $$ = $1; g_ptr_array_add ($$->transitions, $2 /* steal */); }
;

/* Returns a GPtrArray of interface names (strings). */
InterfaceNameList: DBusInterfaceName						{
											$$ = g_ptr_array_new_with_free_func (g_free);
											g_ptr_array_add ($$, $1 /* steal */);
										}
                 | InterfaceNameList ',' DBusInterfaceName			{ $$ = $1; g_ptr_array_add ($$, $3 /* steal */); }
;

/* Returns a new string containing the interface name. */
DBusInterfaceName: DBUS_NAME							{ $$ = $1; /* steal ownership from flex */ }
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

/* Returns a new GHashTable mapping variable names (strings) to DfsmAstDataStructures. */
DataList: /* empty */
		{
			$$ = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_object_unref);
		}
        | VariableName '=' FuzzyDataStructure
		{
			$$ = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_object_unref);
			g_hash_table_insert ($$, $1 /* steal ownership from flex */, $3 /* steal ownership from bison */);
		}
        | VariableName '=' FuzzyDataStructure ';' DataList
		{
			$$ = $5;
			g_hash_table_insert ($$, $1 /* steal ownership from flex */, $3 /* steal ownership from bison */);
		}
        | error ';'
		{ $$ = NULL; YYABORT; }
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

/* Returns a new DfsmParserTransitionBlock. */
TransitionBlock:
	TRANSITION StatePairList ON TransitionType L_BRACE
		PreconditionList
		StatementList
	R_BRACE							{ DfsmAstTransition *transition = dfsm_ast_transition_new ($4, $6, $7);
								  $$ = dfsm_parser_transition_block_new (transition, $2);
								  g_object_unref (transition);
								  g_ptr_array_unref ($2); dfsm_parser_transition_details_free ($4);
								  g_ptr_array_unref ($6); g_ptr_array_unref ($7); }
|	TRANSITION StatePairList ON TransitionType L_BRACE
		error
	R_BRACE							{ $$ = NULL;
								  g_ptr_array_unref ($2); dfsm_parser_transition_details_free ($4);
								  YYABORT; }
;

/* Returns a GPtrArray of DfsmParserStatePairs. */
StatePairList: StatePair							{
											$$ = g_ptr_array_new_with_free_func (
												(GDestroyNotify) dfsm_parser_state_pair_free);
											g_ptr_array_add ($$, $1 /* steal */);
										}
             | StatePairList ',' StatePair					{ $$ = $1; g_ptr_array_add ($$, $3 /* steal */); }
;

/* Returns a new DfsmParserStatePair. */
StatePair: FROM StateName TO StateName						{ $$ = dfsm_parser_state_pair_new ($2, $4);
										  g_free ($2); g_free ($4); }
         | INSIDE StateName							{ $$ = dfsm_parser_state_pair_new (NULL, $2);
										  g_free ($2); }
;

/* Returns a new string containing the method name. */
DBusMethodName: IDENTIFIER							{ $$ = $1; /* steal ownership from flex */ }
;

/* Returns a new string containing the property name. */
DBusPropertyName: IDENTIFIER							{ $$ = $1; /* steal ownership from flex */ }
;

/* Returns a DfsmParserTransitionDetails representing the transition type. */
TransitionType: METHOD DBusMethodName				{ $$ = dfsm_parser_transition_details_new (DFSM_PARSER_TRANSITION_METHOD_CALL, $2);
								  g_free ($2); }
              | PROPERTY DBusPropertyName			{ $$ = dfsm_parser_transition_details_new (DFSM_PARSER_TRANSITION_PROPERTY_SET, $2);
								  g_free ($2); }
              | RANDOM						{ $$ = dfsm_parser_transition_details_new (DFSM_PARSER_TRANSITION_ARBITRARY, NULL); }
;

/* Returns a new GPtrArray containing DfsmAstPreconditions. */
PreconditionList: /* empty */							{ $$ = g_ptr_array_new_with_free_func (g_object_unref); }
                | PreconditionList Precondition					{ $$ = $1; g_ptr_array_add ($$, $2 /* steal */); }
;

/* Returns a new DfsmAstPrecondition. */
Precondition: PRECONDITION PreconditionThrow L_BRACE Expression R_BRACE		{ $$ = dfsm_ast_precondition_new ($2, $4);
										  g_free ($2); g_object_unref ($4); }
            | PRECONDITION PreconditionThrow L_BRACE error R_BRACE		{ $$ = NULL; g_free ($2); YYABORT; }
;

/* Returns a new string containing the error name. */
DBusErrorName: IDENTIFIER							{ $$ = $1; /* steal ownership from flex */ }
;

/* Returns a string containing the error name, or NULL for no error. */
PreconditionThrow: /* empty */							{ $$ = NULL; }
                 | THROWING DBusErrorName					{ $$ = $2; /* steal ownership from flex */ }
;

/* Returns a new GPtrArray of DfsmAstStatements. */
StatementList: Statement ';'							{
											$$ = g_ptr_array_new_with_free_func (g_object_unref);
											g_ptr_array_add ($$, $1 /* steal */);
										}
             | StatementList Statement ';'					{ $$ = $1; g_ptr_array_add ($$, $2 /* steal */); }
             | StatementList error ';'						{ $$ = $1; YYABORT; }
;

/* Returns a new string containing the signal name. */
DBusSignalName: IDENTIFIER							{ $$ = $1; /* steal ownership from flex */ }
;

/* Returns a new DfsmAstStatement (or subclass). */
Statement: DataStructure '=' Expression					{ $$ = dfsm_ast_statement_assignment_new ($1, $3);
									  g_object_unref ($1); g_object_unref ($3); }
         | THROW DBusErrorName						{ $$ = dfsm_ast_statement_throw_new ($2); g_free ($2); }
         | EMIT DBusSignalName Expression				{ $$ = dfsm_ast_statement_emit_new ($2, $3); g_free ($2); g_object_unref ($3); }
         | REPLY Expression						{ $$ = dfsm_ast_statement_reply_new ($2); g_object_unref ($2); }
;

/* Returns a new DfsmAstExpression. */
Expression: L_ANGLE Expression R_ANGLE		{ $$ = $2; }
          | L_ANGLE error R_ANGLE		{ $$ = NULL; YYABORT; }
          | FunctionName Expression %prec FUNC	{ $$ = dfsm_ast_expression_function_call_new ($1, $2); g_free ($1); g_object_unref ($2); }
          | FuzzyDataStructure			{ $$ = dfsm_ast_expression_data_structure_new ($1); g_object_unref ($1); }
          | NOT Expression			{ $$ = dfsm_ast_expression_unary_new (DFSM_AST_EXPRESSION_UNARY_NOT, $2); g_object_unref ($2); }
          | Expression TIMES Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_BINARY_TIMES, $1, $3);
						  g_object_unref ($1); g_object_unref ($3); }
          | Expression DIVIDE Expression	{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_BINARY_DIVIDE, $1, $3);
						  g_object_unref ($1); g_object_unref ($3); }
          | Expression MODULUS Expression	{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_BINARY_MODULUS, $1, $3);
						  g_object_unref ($1); g_object_unref ($3); }
          | Expression PLUS Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_BINARY_PLUS, $1, $3);
						  g_object_unref ($1); g_object_unref ($3); }
          | Expression MINUS Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_BINARY_MINUS, $1, $3);
						  g_object_unref ($1); g_object_unref ($3); }
          | Expression LT Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_BINARY_LT, $1, $3);
						  g_object_unref ($1); g_object_unref ($3); }
          | Expression LTE Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_BINARY_LTE, $1, $3);
						  g_object_unref ($1); g_object_unref ($3); }
          | Expression GT Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_BINARY_GT, $1, $3);
						  g_object_unref ($1); g_object_unref ($3); }
          | Expression GTE Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_BINARY_GTE, $1, $3);
						  g_object_unref ($1); g_object_unref ($3); }
          | Expression EQ Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_BINARY_EQ, $1, $3);
						  g_object_unref ($1); g_object_unref ($3); }
          | Expression NEQ Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_BINARY_NEQ, $1, $3);
						  g_object_unref ($1); g_object_unref ($3); }
          | Expression AND Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_BINARY_AND, $1, $3);
						  g_object_unref ($1); g_object_unref ($3); }
          | Expression OR Expression		{ $$ = dfsm_ast_expression_binary_new (DFSM_AST_EXPRESSION_BINARY_OR, $1, $3);
						  g_object_unref ($1); g_object_unref ($3); }
;

/* Returns the function name as a string. */
FunctionName: IDENTIFIER							{ $$ = $1; /* steal ownership from flex */ }
;

/* Returns the variable name as a string. */
VariableName: IDENTIFIER							{ $$ = $1; /* steal ownership from flex */ }
;

/* Returns a new DfsmAstVariable. */
Variable: VariableName								{ $$ = dfsm_ast_variable_new (DFSM_VARIABLE_SCOPE_LOCAL, $1);
										  g_free ($1); }
        | OBJECT DOT VariableName						{ $$ = dfsm_ast_variable_new (DFSM_VARIABLE_SCOPE_OBJECT, $3);
										  g_free ($3); }
;

/* Returns a new DfsmAstDataStructure. */
FuzzyDataStructure: AnnotatedDataStructure					{ $$ = $1; }
                  | AnnotatedDataStructure FUZZY				{ $$ = $1; dfsm_ast_data_structure_set_weight ($$, 1.0); }
                  | AnnotatedDataStructure FUZZY DOUBLE				{ $$ = $1; dfsm_ast_data_structure_set_weight ($$, $3); }
;

/* Returns a new DfsmAstDataStructure. */
AnnotatedDataStructure: DataStructure						{ $$ = $1; }
                      | TYPE_ANNOTATION DataStructure				{ $$ = $2; dfsm_ast_data_structure_set_type_annotation ($$, $1);
										  g_free ($1); }
;

/* Returns a new DfsmAstDataStructure. */
DataStructure: BYTE					{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_BYTE, &$1); }
             | TRUE_LITERAL				{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_BOOLEAN, GUINT_TO_POINTER (TRUE)); }
             | FALSE_LITERAL				{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_BOOLEAN, GUINT_TO_POINTER (FALSE)); }
             | INT16					{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_INT16, &$1); }
             | UINT16					{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_UINT16, &$1); }
             | INT32					{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_INT32, &$1); }
             | UINT32					{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_UINT32, &$1); }
             | INT64					{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_INT64, &$1); }
             | UINT64					{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_UINT64, &$1); }
             | DOUBLE					{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_DOUBLE, &$1); }
             | STRING					{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_STRING, $1); g_free ($1); }
             | '<' Expression '>'			{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_VARIANT, $2); g_object_unref ($2); }
             | '<' error '>'				{ $$ = NULL; YYABORT; }
             | ARRAY_L_BRACKET ArrayList ARRAY_R_BRACKET	{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_ARRAY, $2); g_ptr_array_unref ($2); }
             | ARRAY_L_BRACKET error ARRAY_R_BRACKET		{ $$ = NULL; YYABORT; }
             | L_PAREN StructureList R_PAREN		{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_STRUCT, $2); g_ptr_array_unref ($2); }
             | L_PAREN error R_PAREN			{ $$ = NULL; YYABORT; }
             | L_BRACE DictionaryList R_BRACE		{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_DICT, $2); g_ptr_array_unref ($2); }
             | L_BRACE error R_BRACE			{ $$ = NULL; YYABORT; }
             | Variable					{ $$ = dfsm_ast_data_structure_new (DFSM_AST_DATA_VARIABLE, $1); g_object_unref ($1); }
;

/* Returns a new GPtrArray of DfsmAstExpressions */
ArrayList: ArrayListInner							{ $$ = $1; }
         | ArrayListInner Expression						{ $$ = $1; g_ptr_array_add ($$, $2 /* steal */); }
;
ArrayListInner: /* empty */							{ $$ = g_ptr_array_new_with_free_func (g_object_unref); }
              | ArrayListInner Expression ','					{ $$ = $1; g_ptr_array_add ($$, $2 /* steal */); }
;

/* Returns a new GPtrArray of DfsmAstDictionaryEntrys */
DictionaryList: DictionaryListInner						{ $$ = $1; }
              | DictionaryListInner Expression ':' Expression			{
											$$ = $1;
											g_ptr_array_add ($$, dfsm_ast_dictionary_entry_new ($2, $4));
											g_object_unref ($2); g_object_unref ($4);
										}
;
DictionaryListInner: /* empty */			{
								$$ = g_ptr_array_new_with_free_func ((GDestroyNotify) dfsm_ast_dictionary_entry_free);
							}
                   | DictionaryListInner Expression ':' Expression ','		{
											$$ = $1;
											g_ptr_array_add ($$, dfsm_ast_dictionary_entry_new ($2, $4));
											g_object_unref ($2); g_object_unref ($4);
										}
;

/* Returns a new GPtrArray of DfsmAstExpressions */
StructureList: StructureListInner						{ $$ = $1; }
             | StructureListInner Expression					{ $$ = $1; g_ptr_array_add ($$, $2 /* steal */); }
;
StructureListInner: /* empty */							{ $$ = g_ptr_array_new_with_free_func (g_object_unref); }
                  | StructureListInner Expression ','				{ $$ = $1; g_ptr_array_add ($$, $2 /* steal */); }
;

%%

GPtrArray *
dfsm_bison_parse (GDBusNodeInfo *dbus_node_info, const gchar *source_buf, GError **error)
{
	GError *child_error = NULL;
	DfsmParserData data = { 0, };
	int result;
	GPtrArray *retval = NULL;

	/* Set up the parsing environment. */
	data.yyscanner = NULL;
	data.object_array = NULL;
	data.dbus_node_info = dbus_node_info;

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
		retval = NULL;
	} else if (result == 2) {
		/* OOM */
		retval = NULL;

		/* If an error had already been set, return that; otherwise report the OOM. */
		if (child_error == NULL) {
			g_set_error (&child_error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_OOM, _("Out of memory."));
		}
	} else {
		g_assert_not_reached ();
	}

	/* Output */
	g_assert ((retval != NULL && child_error == NULL) || (retval == NULL && child_error != NULL));

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		child_error = NULL;
	}

	/* Tidy up */
	g_clear_error (&child_error);
	if (data.object_array != NULL) {
		g_ptr_array_unref (data.object_array);
	}

	return retval;
}

static void
yyerror (YYLTYPE *yylloc, DfsmParserData *parser_data, GError **error, const char *message)
{
	g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_SYNTAX, _("Syntax error at %u:%u–%u:%u: %s"),
	             yylloc->first_line, yylloc->first_column + 1 /* zero-based */,
	             yylloc->last_line, yylloc->last_column + 1 /* zero-based */, message);
}
