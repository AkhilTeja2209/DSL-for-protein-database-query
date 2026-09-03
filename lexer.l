/* lexer.l
 * -----------------------------------------------------------------------
 * Phase 1: Lexical Analysis.
 *
 * Recognizes ProteinDSL keywords, identifiers, string/number literals and
 * comparison operators, and hands tokens to the Bison parser (parser.y).
 *
 * Keywords are matched before the identifier rule, so `LOAD` lexes as the
 * LOAD token while `loaded_at` lexes as an IDENTIFIER (flex prefers the
 * longest match, and breaks ties by rule order).
 * ----------------------------------------------------------------------- */

%{
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "ast.h"
#include "parser.tab.hpp"

int lex_errors = 0;
%}

%option noyywrap
%option yylineno

%%

[ \t\r\n]+              { /* whitespace, including newlines between clauses */ }
"//".*                  { /* line comment - convenience, not in the original spec */ }

"LOAD"                  { return LOAD; }
"UNIPROT"               { return UNIPROT; }
"FIND"                  { return FIND; }
"WHERE"                 { return WHERE; }
"AND"                   { return AND; }
"OR"                    { return OR; }
"COUNT"                 { return COUNT; }
"DISPLAY"               { return DISPLAY; }
"SORT"                  { return SORT; }
"BY"                    { return BY; }
"ASC"                   { return ASC; }
"DESC"                  { return DESC; }
"TOP"                   { return TOP; }
"SEARCH"                { return SEARCH; }

">="                    { return GE; }
"<="                    { return LE; }
"!="                    { return NE; }
"="                     { return EQ; }
">"                     { return GT; }
"<"                     { return LT; }

\"[^\"]*\"              {
                            yylval.strval = strdup(yytext + 1);
                            yylval.strval[strlen(yylval.strval) - 1] = '\0';
                            return STRING;
                         }

[0-9]+(\.[0-9]+)?       { yylval.numval = atof(yytext); return NUMBER; }

[A-Za-z_][A-Za-z0-9_]*  { yylval.strval = strdup(yytext); return IDENTIFIER; }

.                        {
                            char buf[128];
                            snprintf(buf, sizeof(buf),
                                     "Lexical Error: unexpected character '%s' at line %d",
                                     yytext, yylineno);
                            g_parseErrors.push_back(buf);
                            fprintf(stderr, "%s\n", buf);
                            lex_errors++;
                         }
%%
