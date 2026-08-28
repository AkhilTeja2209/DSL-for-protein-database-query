/* lexer.l
 * -----------------------------------------------------------------------
 * Phase 1: Lexical Analysis (see project plan, slide 9).
 *
 * Recognizes ProteinDSL keywords, identifiers, string/number literals and
 * comparison operators, and hands tokens to the Bison parser (parser.y).
 * Every keyword listed in "Core Language Features" (slide 6) is tokenized
 * here, even though the parser (Phase 2) does not yet accept all of them
 * in a grammar rule -- see README "Current Status".
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
"FIND"                  { return FIND; }
"WHERE"                 { return WHERE; }
"AND"                   { return AND; }
"OR"                    { return OR; }
"COUNT"                 { return COUNT; }
"DISPLAY"               { return DISPLAY; }
"SORT"                  { return SORT; }
"BY"                    { return BY; }
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
                            fprintf(stderr,
                                    "Lexical Error: unexpected character '%s' at line %d\n",
                                    yytext, yylineno);
                            lex_errors++;
                         }

%%
