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
	static int regexp_caller = 0;

	#define MAX_STRING_CONST_SIZE 4096
	static char string_buf[MAX_STRING_CONST_SIZE + 1 /* nul delimiter */];
	static char *string_buf_ptr = NULL;

	#define MAX_REGEXP_CONST_SIZE 4096
	static char regexp_buf[MAX_REGEXP_CONST_SIZE + 1 /* nul delimiter */];
	static char *regexp_buf_ptr = NULL;

	#define YY_INPUT(buf, result, max_size) result = yy_input_proc (buf, max_size, yyscanner)
	static int yy_input_proc (gchar *out_buf, gint size, yyscan_t yyscanner);

	/* Flex is a bit lax about including default cases in switch statements, so we ignore such errors in this file. */
	#ifdef __GNUC__
		#pragma GCC diagnostic ignored "-Wswitch-default"
	#endif
%}

%x comment
%x string
%x regexp

/* Definitions, mainly extracted from http://dbus.freedesktop.org/doc/dbus-specification.html. */
DBUS_OBJECT_PATH_ELEMENT		[A-Za-z0-9_]+
DBUS_OBJECT_PATH			\/({DBUS_OBJECT_PATH_ELEMENT}(\/{DBUS_OBJECT_PATH_ELEMENT})*)?

/* Additional restrictions not encoded here:
 *  • Must be UTF-8.
 *  • Must not exceed 255 characters in length.
 */
DBUS_INTERFACE_NAME_ELEMENT		[A-Za-z_]+[A-Za-z0-9_]*
DBUS_INTERFACE_NAME			{DBUS_INTERFACE_NAME_ELEMENT}\.{DBUS_INTERFACE_NAME_ELEMENT}(\.{DBUS_INTERFACE_NAME_ELEMENT})*

/* Additional restrictions not encoded here:
 *  • Must be UTF-8.
 *  • Must not exceed 255 characters in length.
 */
DBUS_UNIQUE_BUS_NAME_ELEMENT		[A-Za-z0-9_-]+
DBUS_UNIQUE_BUS_NAME			:{DBUS_UNIQUE_BUS_NAME_ELEMENT}\.{DBUS_UNIQUE_BUS_NAME_ELEMENT}(\.{DBUS_UNIQUE_BUS_NAME_ELEMENT})*
DBUS_WELL_KNOWN_BUS_NAME_ELEMENT	[A-Za-z_-]+[A-Za-z0-9_-]*
DBUS_WELL_KNOWN_BUS_NAME		{DBUS_WELL_KNOWN_BUS_NAME_ELEMENT}\.{DBUS_WELL_KNOWN_BUS_NAME_ELEMENT}(\.{DBUS_WELL_KNOWN_BUS_NAME_ELEMENT})*

/* Additional restrictions not encoded here:
 *  • Must be well-formed and encode a complete type.
 *  • Must not exceed 255 characters in length.
 *  • Must be nul-terminated.
 *
 * Note that we prefix it with a colon so that it doesn't look like an identifier or get confused with opening braces.
 */
DBUS_TYPE_SIGNATURE			:[ \t\n\r\v\f]*[\(\)\{\}ybnqiuxtdsogarvehm\*\?@&^]+

/* Includes variable names, state names, function names, D-Bus member names, D-Bus error names.
 *
 * Additional restrictions on D-Bus member and error names not encoded here:
 *  • Must be UTF-8.
 *  • Must not exceed 255 characters in length.
 */
IDENTIFIER				[A-Za-z_]+[A-Za-z0-9_]*

BYTE					[0-9]+y
INT16					(\+-)?[0-9]+n
UINT16					[0-9]+q
INT32					(\+-)?[0-9]+i
UINT32					[0-9]+u
INT64					(\+-)?[0-9]+x
UINT64					[0-9]+t
DOUBLE					(\+-)?[0-9]+\.[0-9]+d

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

"#" { regexp_caller = YYSTATE; regexp_buf_ptr = regexp_buf; BEGIN(regexp); }
<regexp>{
	"#" { /* closing delimiter */
		BEGIN(regexp_caller);
		*regexp_buf_ptr = '\0';
		yylval->str = strdup (regexp_buf);
		return REGEXP;
	}

	"\n" {
		/* Unterminated regexp constant */
		return FLEX_ERROR;
	}

	\\(.|\n) { if (regexp_buf_ptr - regexp_buf + 1 > MAX_REGEXP_CONST_SIZE) { return FLEX_ERROR; } *regexp_buf_ptr++ = yytext[1]; }
	[^\\\n\"]+ {
		size_t yytext_len = strlen (yytext);

		if (regexp_buf_ptr - regexp_buf + yytext_len > MAX_REGEXP_CONST_SIZE) {
			return FLEX_ERROR;
		}

		strncpy (regexp_buf_ptr, yytext, yytext_len);
		regexp_buf_ptr += yytext_len;
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
"on" { return ON; }
"method" { return METHOD; }
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
"<" { return LT; }
"<=" { return LTE; }
">" { return GT; }
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

{DBUS_OBJECT_PATH} { yylval->str = strdup (yytext); return DBUS_OBJECT_PATH; }
{DBUS_INTERFACE_NAME} { yylval->str = strdup (yytext); return DBUS_INTERFACE_NAME; }
{DBUS_TYPE_SIGNATURE} {
	/* Strip the leading colon and whitespace */
	do {
		yytext++;
	} while (isspace (*yytext));

	yylval->str = strdup (yytext);
	return DBUS_TYPE_SIGNATURE;
}

{IDENTIFIER} { yylval->str = strdup (yytext); return IDENTIFIER; }

{BYTE} { yylval->unsigned_integer = g_ascii_strtoull (yytext, NULL, 10); return BYTE; }
{INT16} { yylval->integer = g_ascii_strtoll (yytext, NULL, 10); return INT16; }
{UINT16} { yylval->unsigned_integer = g_ascii_strtoull (yytext, NULL, 10); return UINT16; }
{INT32} { yylval->integer = g_ascii_strtoll (yytext, NULL, 10); return INT32; }
{UINT32} { yylval->unsigned_integer = g_ascii_strtoull (yytext, NULL, 10); return UINT32; }
{INT64} { yylval->integer = g_ascii_strtoll (yytext, NULL, 10); return INT64; }
{UINT64} { yylval->unsigned_integer = g_ascii_strtoull (yytext, NULL, 10); return UINT64; }
{DOUBLE} { yylval->flt = g_ascii_strtod (yytext, NULL); return DOUBLE; }

[ \t\n\r\v\f] { /* Ignore whitespace */ }

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
