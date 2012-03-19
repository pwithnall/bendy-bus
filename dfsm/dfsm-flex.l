%option reentrant
%option bison-bridge
%option bison-locations
%option noyywrap
%option nounput
%option noinput
%option yylineno
%option extra-type="DfsmParserData*"

/* Flex erroneously redeclares isatty() otherwise. */
%option nounistd

%{
	#include <ctype.h>
	#include <glib.h>

	#include "dfsm-ast.h"
	#include "dfsm-parser.h"
	#include "dfsm_libdfsm_la-dfsm-bison.h"

	static int comment_caller = 0;
	static int string_caller = 0;

	#define MAX_STRING_CONST_SIZE 4096
	static char string_buf[MAX_STRING_CONST_SIZE + 1 /* nul delimiter */];
	static char *string_buf_ptr = NULL;

	#define YY_INPUT(buf, result, max_size) result = yy_input_proc (buf, max_size, yyscanner)
	static int yy_input_proc (gchar *out_buf, gint size, yyscan_t yyscanner);

	/* Flex is a bit lax about including default cases in switch statements, so we ignore such errors in this file.
	 * It also (despite our best efforts with "%option nounistd") redeclares isatty(). Ignore that too. */
	#ifdef __GNUC__
		#pragma GCC diagnostic ignored "-Wswitch-default"
		#pragma GCC diagnostic ignored "-Wredundant-decls"
	#endif

	/* Locations */
	#define YY_USER_ACTION \
		yylloc->first_line = yylloc->last_line = yylineno; \
		yylloc->first_column = yycolumn; \
		yylloc->last_column = yycolumn + yyleng - 1; \
		yycolumn += yyleng;

	static unsigned int tab_width = 8; /* default */

	#define YY_USER_INIT \
		{ \
			/* We parse the TABSIZE environment variable, as used by ls(1), to get the user's tab size preference. */ \
			char *tabsize = getenv ("TABSIZE"); \
			if (tabsize != NULL) { \
				char *tabsize_end = NULL; \
				unsigned long int new_tab_width = strtoul (tabsize, &tabsize_end, 10); \
				if (*tabsize != '\0' && tabsize_end != NULL && *tabsize_end == '\0' && new_tab_width <= UINT_MAX) { \
					/* Valid. */ \
					tab_width = new_tab_width; \
				} \
			} \
		}
%}

%x comment
%x string

/* Definitions, mainly extracted from http://dbus.freedesktop.org/doc/dbus-specification.html. */
DBUS_OBJECT_PATH_ELEMENT		[A-Za-z0-9_]+
DBUS_OBJECT_PATH			\/({DBUS_OBJECT_PATH_ELEMENT}(\/{DBUS_OBJECT_PATH_ELEMENT})*)?

/* Includes D-Bus interface names and well-known bus names.
 *
 * Additional restrictions not encoded here:
 *  • Must be UTF-8.
 *  • Must not exceed 255 characters in length.
 *  • Interface name elements can't contain hyphens.
 */
DBUS_NAME_ELEMENT			[A-Za-z_-]+[A-Za-z0-9_-]*
DBUS_NAME				{DBUS_NAME_ELEMENT}\.{DBUS_NAME_ELEMENT}(\.{DBUS_NAME_ELEMENT})*

/* Includes variable names, state names, function names, D-Bus member names, D-Bus error names.
 *
 * Additional restrictions on D-Bus member and error names not encoded here:
 *  • Must be UTF-8.
 *  • Must not exceed 255 characters in length.
 */
IDENTIFIER				[A-Za-z_]+[A-Za-z0-9_]*

INTEGER					[\+-]?[0-9]+
DOUBLE					[\+-]?[0-9]+\.[0-9]+

TYPE_ANNOTATION				@[\(\)\{\}ybnqiuxtdsogarvehm\*\?@&^]+[ \t\n\r\v\f]+

%%

"/*" { comment_caller = YYSTATE; BEGIN(comment); }
<comment>{
	[^*]* { /* Eat anything not a '*' */ }
	"*"+[^*/]* { /* Eat any '*'s not followed by '/' */ }
	"*"+"/" { BEGIN(comment_caller); /* close comment */ }
}

"\"" { string_caller = YYSTATE; string_buf_ptr = string_buf; BEGIN(string); }
<string>{
	"\"" { /* closing quote */
		BEGIN(string_caller);
		*string_buf_ptr = '\0';
		yylval->str = strdup (string_buf);
		return STRING;
	}

	"\n" {
		/* Unterminated string constant */
		return FLEX_ERROR;
	}

	"\\n" { if (string_buf_ptr - string_buf + 1 > MAX_STRING_CONST_SIZE) { return FLEX_ERROR; } *string_buf_ptr++ = '\n'; }
	"\\t" { if (string_buf_ptr - string_buf + 1 > MAX_STRING_CONST_SIZE) { return FLEX_ERROR; } *string_buf_ptr++ = '\t'; }
	"\\r" { if (string_buf_ptr - string_buf + 1 > MAX_STRING_CONST_SIZE) { return FLEX_ERROR; } *string_buf_ptr++ = '\r'; }
	"\\b" { if (string_buf_ptr - string_buf + 1 > MAX_STRING_CONST_SIZE) { return FLEX_ERROR; } *string_buf_ptr++ = '\b'; }
	"\\f" { if (string_buf_ptr - string_buf + 1 > MAX_STRING_CONST_SIZE) { return FLEX_ERROR; } *string_buf_ptr++ = '\f'; }

	\\(.|\n) { if (string_buf_ptr - string_buf + 1 > MAX_STRING_CONST_SIZE) { return FLEX_ERROR; } *string_buf_ptr++ = yytext[1]; }
	[^\\\n\"]+ {
		size_t yytext_len = strlen (yytext);

		if (string_buf_ptr - string_buf + yytext_len > MAX_STRING_CONST_SIZE) {
			return FLEX_ERROR;
		}

		strncpy (string_buf_ptr, yytext, yytext_len);
		string_buf_ptr += yytext_len;
	}
}

"object" { return OBJECT; }
"at" { return AT; }
"implements" { return IMPLEMENTS; }
"data" { return DATA; }
"states" { return STATES; }
"transition" { return TRANSITION; }
"from" { return FROM; }
"to" { return TO; }
"inside" { return INSIDE; }
"on" { return ON; }
"method" { return METHOD; }
"property" { return PROPERTY; }
"random" { return RANDOM; }
"precondition" { return PRECONDITION; }
"throwing" { return THROWING; }
"throw" { return THROW; }
"emit" { return EMIT; }
"reply" { return REPLY; }
"!" { return NOT; }
"*" { return TIMES; }
"/" { return DIVIDE; }
"%" { return MODULUS; }
"+" { return PLUS; }
"-" { return MINUS; }
"<~" { return LT; }
"<=" { return LTE; }
"~>" { return GT; }
">=" { return GTE; }
"==" { return EQ; }
"!=" { return NEQ; }
"&&" { return AND; }
"||" { return OR; }
"true" { return TRUE_LITERAL; }
"false" { return FALSE_LITERAL; }
"[" { return ARRAY_L_BRACKET; }
"]" { return ARRAY_R_BRACKET; }
"{" { return L_BRACE; }
"}" { return R_BRACE; }
"(" { return L_PAREN; }
")" { return R_PAREN; }
"⟨" { return L_ANGLE; }
"⟩" { return R_ANGLE; }
"?" { return FUZZY; }
"->" { return DOT; }

{TYPE_ANNOTATION} {
	char *i;

	/* String the leading at-sign and trailing whitespace. */
	yytext++;
	yyleng--;

	i = yytext + yyleng;
	do {
		i--;
	} while (isspace (*i));

	*(++i) = '\0';

	/* Store the annotation. */
	yylval->str = strdup (yytext);

	return TYPE_ANNOTATION;
}

{DBUS_OBJECT_PATH} { yylval->str = strdup (yytext); return DBUS_OBJECT_PATH; }
{DBUS_NAME} { yylval->str = strdup (yytext); return DBUS_NAME; }

{IDENTIFIER} { yylval->str = strdup (yytext); return IDENTIFIER; }

{INTEGER} { yylval->str = strdup (yytext); return INTEGER; /* return strings because it would be icky to detect overflow here and propagate the error
                                                            * all the way through to the AST */ }
{DOUBLE} { yylval->str = strdup (yytext); return DOUBLE; }

[\t] { /* Ignore tabs, but ensure they change the source code location appropriately. */ yycolumn += tab_width - 1 /* will be counted once later */; }
[ \n\r\v\f] { /* Ignore whitespace */ }

. { return (int) yytext[0]; } /* Unmatched characters */

%%

static int
yy_input_proc (gchar *out_buf, gint size, yyscan_t yyscanner)
{
	gunichar unichar;
	DfsmParserData *data = (DfsmParserData*) yyget_extra (yyscanner);

	if (data->source_pos >= data->source_len) {
		return 0;
	}

	/* Copy up to size bytes' worth of complete UTF-8 characters to out_buf. We do this by validating the byte at (source_pos + size);
	 * if it's the start a valid UTF-8 character, we return exactly size bytes. Otherwise, we backtrack to the start of the previous
	 * character and return slightly fewer bytes. */
	size = MIN (size, data->source_len - data->source_pos);

	unichar = g_utf8_get_char_validated (data->source_buf + data->source_pos + size, -1);
	if (unichar == (gunichar) -2 || unichar == (gunichar) -1) {
		size = g_utf8_prev_char (data->source_buf + data->source_pos + size) - data->source_buf - data->source_pos;
	}

	memcpy (out_buf, data->source_buf + data->source_pos, size);
	data->source_pos += size;

	return size;
}
