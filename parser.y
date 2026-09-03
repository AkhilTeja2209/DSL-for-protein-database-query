/* parser.y
 * -----------------------------------------------------------------------
 * Phase 2: Syntax Analysis.
 *
 * Grammar for ProteinDSL:
 *
 *   program     -> statement*
 *   statement   -> LOAD STRING
 *                | LOAD UNIPROT STRING top_opt
 *                | FIND IDENTIFIER search_opt where_opt sort_opt top_opt output_opt
 *   search_opt  -> SEARCH STRING | (empty)
 *   where_opt   -> WHERE or_expr | (empty)
 *   or_expr     -> or_expr OR and_expr | and_expr
 *   and_expr    -> and_expr AND condition | condition
 *   condition   -> IDENTIFIER cmp_op value
 *   cmp_op      -> '=' | '>' | '<' | '>=' | '<=' | '!='
 *   value       -> STRING | NUMBER
 *   sort_opt    -> SORT BY IDENTIFIER dir_opt | (empty)
 *   dir_opt     -> ASC | DESC | (empty, meaning ASC)
 *   top_opt     -> TOP NUMBER | (empty)
 *   output_opt  -> DISPLAY id_list | COUNT | (empty, meaning all columns)
 *
 * AND binds tighter than OR, so the WHERE clause is collected directly into
 * the disjunctive normal form the AST stores (see ast.h).
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
std::vector<std::string> g_parseErrors;

// Sentinel for an absent `TOP n` clause. TOP is validated as a positive
// integer during semantic analysis, so -1 can never be a real value.
static const double kNoTop = -1.0;

%}

%union {
    char*                       strval;
    double                      numval;
    std::vector<Condition>*     andgroup;
    std::vector<AndGroup>*      whereclause;
    Condition*                  cond;
    std::vector<std::string>*   idlist;
    CompareOp                   cmpop;
    SortDir                     sortdir;
    SortSpec*                   sortspec;
    OutputSpec*                 outspec;
    ValueLiteral*               vallit;
}

%token LOAD UNIPROT FIND WHERE AND OR COUNT DISPLAY SORT BY ASC DESC TOP SEARCH
%token GE LE NE EQ GT LT
%token <strval> IDENTIFIER STRING
%token <numval> NUMBER

%type <whereclause> where_opt or_expr
%type <andgroup>    and_expr
%type <cond>        condition
%type <idlist>      id_list
%type <cmpop>       cmp_op
%type <vallit>      value
%type <strval>      search_opt
%type <sortspec>    sort_opt
%type <sortdir>     dir_opt
%type <numval>      top_opt
%type <outspec>     output_opt

%%

program:
      %empty     
    | program statement
    ;

statement:
      LOAD STRING {
          Statement s;
          s.type = StmtType::LOAD;
          s.loadSource = LoadSource::FILE;
          s.loadFile = $2;
          free($2);
          g_program.push_back(s);
      }
    | LOAD UNIPROT STRING top_opt {
          Statement s;
          s.type = StmtType::LOAD;
          s.loadSource = LoadSource::UNIPROT;
          s.uniprotQuery = $3;
          free($3);
          if ($4 != kNoTop) s.uniprotLimit = static_cast<long>($4);
          g_program.push_back(s);
      }
    | FIND IDENTIFIER search_opt where_opt sort_opt top_opt output_opt {
          Statement s;
          s.type = StmtType::FIND;
          s.entity = $2;
          free($2);

          if ($3) { s.hasSearch = true; s.searchTerm = $3; free($3); }
          if ($4) { s.where = *$4; delete $4; }
          if ($5) {
              s.hasSort = true;
              s.sortField = $5->field;
              s.sortDir = $5->dir;
              delete $5;
          }
          if ($6 != kNoTop) { s.hasTop = true; s.topN = static_cast<long>($6); }
          if ($7) {
              s.output = $7->kind;
              s.displayFields = $7->fields;
              delete $7;
          }
          g_program.push_back(s);
      }
    ;

search_opt:
      %empty          { $$ = nullptr; }
    | SEARCH STRING   { $$ = $2; }
    ;

where_opt:
      %empty          { $$ = nullptr; }
    | WHERE or_expr   { $$ = $2; }
    ;

/* OR of AND-groups: each and_expr becomes one group in the DNF. */
or_expr:
      and_expr {
          $$ = new WhereClause();
          $$->push_back(*$1);
          delete $1;
      }
    | or_expr OR and_expr {
          $1->push_back(*$3);
          delete $3;
          $$ = $1;
      }
    ;

and_expr:
      condition {
          $$ = new AndGroup();
          $$->push_back(*$1);
          delete $1;
      }
    | and_expr AND condition {
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
          $$->value = $3->text;
          // Which token the lexer actually saw -- NOT a guess from the text.
          // This is what lets semantic analysis reject `length = "300"`.
          $$->isNumeric = $3->isNumeric;
          free($1);
          delete $3;
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
      STRING {
          $$ = new ValueLiteral{ std::string($1), false };
          free($1);
      }
    | NUMBER {
          // Format without a forced ".000000" for whole numbers like 300.
          char buf[64];
          if ($1 == static_cast<long long>($1)) {
              snprintf(buf, sizeof(buf), "%lld", static_cast<long long>($1));
          } else {
              snprintf(buf, sizeof(buf), "%g", $1);
          }
          $$ = new ValueLiteral{ std::string(buf), true };
      }
    ;

sort_opt:
      %empty                          { $$ = nullptr; }
    | SORT BY IDENTIFIER dir_opt {
          $$ = new SortSpec();
          $$->field = $3;
          $$->dir = $4;
          free($3);
      }
    ;

dir_opt:
      %empty       { $$ = SortDir::ASC; }
    | ASC          { $$ = SortDir::ASC; }
    | DESC         { $$ = SortDir::DESC; }
    ;

top_opt:
      %empty       { $$ = kNoTop; }
    | TOP NUMBER   { $$ = $2; }
    ;

output_opt:
      %empty           { $$ = nullptr; }
    | COUNT {
          $$ = new OutputSpec();
          $$->kind = OutputKind::COUNT;
      }
    | DISPLAY id_list {
          $$ = new OutputSpec();
          $$->kind = OutputKind::DISPLAY;
          $$->fields = *$2;
          delete $2;
      }
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

// Bison calls this on a parse error. The message is recorded as well as
// printed, so that --json (and therefore the web playground) reports the line
// number and reason instead of a bare "Parsing failed."
void yyerror(const char* msg) {
    const std::string text = "Syntax Error at line " + std::to_string(yylineno) +
                             ": " + msg;
    g_parseErrors.push_back(text);
    fprintf(stderr, "%s\n", text.c_str());
}
