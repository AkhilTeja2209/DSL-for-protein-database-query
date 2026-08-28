/* parser.y
 * -----------------------------------------------------------------------
 * Phase 2: Syntax Analysis (see project plan, slide 10).
 *
 * Grammar for the subset of ProteinDSL demonstrated so far:
 *
 *   program    -> statement*
 *   statement  -> LOAD STRING
 *               | FIND IDENTIFIER where_opt display_opt
 *   where_opt  -> WHERE cond_list | (empty)
 *   cond_list  -> cond_list AND condition | condition
 *   condition  -> IDENTIFIER cmp_op value
 *   cmp_op     -> '=' | '>' | '<' | '>=' | '<=' | '!='
 *   value      -> STRING | NUMBER
 *   display_opt-> DISPLAY IDENTIFIER+ | (empty)
 *
 * This covers the example program from slide 5 end-to-end and builds a
 * real AST (ast.h) for it. COUNT, SORT BY, TOP, SEARCH and OR are already
 * tokenized by the lexer but intentionally not wired into this grammar
 * yet -- extending cond_list/statement to cover them is the next step in
 * the implementation plan (slide 17, steps 3-4). See README.
 * ----------------------------------------------------------------------- */

%{
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "ast.h"

int yylex();
void yyerror(const char* msg);

extern int yylineno;

Program g_program;

%}

%union {
    char*                       strval;
    double                      numval;
    std::vector<Condition>*     condlist;
    Condition*                  cond;
    std::vector<std::string>*   idlist;
    CompareOp                   cmpop;
}

%token LOAD FIND WHERE AND OR COUNT DISPLAY SORT BY TOP SEARCH
%token GE LE NE EQ GT LT
%token <strval> IDENTIFIER STRING
%token <numval> NUMBER

%type <condlist> where_opt cond_list
%type <cond>     condition
%type <idlist>   display_opt id_list
%type <cmpop>    cmp_op
%type <strval>   value

%%

program:
      /* empty */
    | program statement
    ;

statement:
      LOAD STRING {
          Statement s;
          s.type = StmtType::LOAD;
          s.loadFile = $2;
          free($2);
          g_program.push_back(s);
      }
    | FIND IDENTIFIER where_opt display_opt {
          Statement s;
          s.type = StmtType::FIND;
          s.entity = $2;
          free($2);
          if ($3) { s.conditions = *$3; delete $3; }
          if ($4) { s.displayFields = *$4; delete $4; }
          g_program.push_back(s);
      }
    ;

where_opt:
      /* empty */        { $$ = nullptr; }
    | WHERE cond_list     { $$ = $2; }
    ;

cond_list:
      condition {
          $$ = new std::vector<Condition>();
          $$->push_back(*$1);
          delete $1;
      }
    | cond_list AND condition {
          $1->push_back(*$3);
          delete $3;
          $$ = $1;
      }
    ;

condition:
      IDENTIFIER cmp_op value {
          $$ = new Condition();
          $$->field = $1;
          $$->op = $2;
          $$->value = $3;
          // Heuristic type tag for the demo printer / future semantic pass:
          // a value is "numeric" if it parses entirely as a number.
          char* end = nullptr;
          std::strtod($3, &end);
          $$->isNumeric = (end != $3 && *end == '\0');
          free($1);
          free($3);
      }
    ;

cmp_op:
      EQ { $$ = CompareOp::EQ; }
    | GT { $$ = CompareOp::GT; }
    | LT { $$ = CompareOp::LT; }
    | GE { $$ = CompareOp::GE; }
    | LE { $$ = CompareOp::LE; }
    | NE { $$ = CompareOp::NE; }
    ;

value:
      STRING { $$ = $1; }
    | NUMBER {
          // Format without a forced ".000000" for whole numbers like 300.
          char buf[64];
          if ($1 == static_cast<long long>($1)) {
              snprintf(buf, sizeof(buf), "%lld", static_cast<long long>($1));
          } else {
              snprintf(buf, sizeof(buf), "%g", $1);
          }
          $$ = strdup(buf);
      }
    ;

display_opt:
      /* empty */    { $$ = nullptr; }
    | DISPLAY id_list { $$ = $2; }
    ;

id_list:
      IDENTIFIER {
          $$ = new std::vector<std::string>();
          $$->push_back($1);
          free($1);
      }
    | id_list IDENTIFIER {
          $1->push_back($2);
          free($2);
          $$ = $1;
      }
    ;

%%

void yyerror(const char* msg) {
    fprintf(stderr, "Syntax Error at line %d: %s\n", yylineno, msg);
}
