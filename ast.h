// ast.h
// -----------------------------------------------------------------------
// Internal representation for parsed ProteinDSL programs.
//
// Pipeline stage: Parser (Bison) -> AST -> Semantic Analyzer -> Executor
// See System Architecture in the README.
//
// A WHERE clause is stored in disjunctive normal form: an OR of AND-groups.
// `AND` binds tighter than `OR`, so
//     WHERE a AND b OR c
// parses as (a AND b) OR (c) and is stored as {{a, b}, {c}}.
// -----------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>
#include <iosfwd>

enum class StmtType { LOAD, FIND };

// LOAD "file.csv"  vs  LOAD UNIPROT "query"
enum class LoadSource { FILE, UNIPROT };

enum class CompareOp { EQ, GT, LT, GE, LE, NE };

enum class SortDir { ASC, DESC };

// What a FIND prints: a table of every column, a table of the DISPLAY-selected
// columns, or just the number of matching records.
enum class OutputKind { DEFAULT, DISPLAY, COUNT };

// A literal on the right-hand side of a condition. `isNumeric` records which
// token the parser actually saw, so `length = "300"` is a type error rather
// than being silently accepted.
struct ValueLiteral {
    std::string text;
    bool        isNumeric = false;
};

// A single WHERE condition, e.g. `length > 300` or `organism = "Human"`.
struct Condition {
    std::string field;
    CompareOp   op = CompareOp::EQ;
    std::string value;              // raw literal text
    bool        isNumeric = false;  // true if the literal was a NUMBER token
};

using AndGroup    = std::vector<Condition>;
using WhereClause = std::vector<AndGroup>;   // OR of AND-groups (see above)

// SORT BY <field> [ASC|DESC]
struct SortSpec {
    std::string field;
    SortDir     dir = SortDir::ASC;
};

// DISPLAY <fields> | COUNT | (neither)
struct OutputSpec {
    OutputKind               kind = OutputKind::DEFAULT;
    std::vector<std::string> fields;
};

// One DSL statement. Only the fields relevant to `type` are populated.
struct Statement {
    StmtType type = StmtType::LOAD;

    // --- LOAD ---
    LoadSource  loadSource = LoadSource::FILE;
    std::string loadFile;       // LOAD "dataset/proteins.csv"
    std::string uniprotQuery;   // LOAD UNIPROT "insulin AND organism_id:9606"
    long        uniprotLimit = 50;  // LOAD UNIPROT "..." TOP <n>

    // --- FIND <entity> [SEARCH s] [WHERE c] [SORT BY f [dir]] [TOP n] [out] ---
    std::string entity;

    bool        hasSearch = false;
    std::string searchTerm;

    WhereClause where;

    bool        hasSort = false;
    std::string sortField;
    SortDir     sortDir = SortDir::ASC;

    bool        hasTop = false;
    long        topN = 0;

    OutputKind               output = OutputKind::DEFAULT;
    std::vector<std::string> displayFields;  // only when output == DISPLAY
};

using Program = std::vector<Statement>;

// Built up by the Bison parser as it reduces the input; main.cpp reads this
// once yyparse() returns 0 (success).
extern Program g_program;

// Lexical and syntax errors collected during phases 1 and 2. They are also
// written to stderr as they happen; keeping a copy lets --json report them
// in the same structured document as the later phases.
extern std::vector<std::string> g_parseErrors;

// Pretty-prints the parsed program. Writing to an ostream (rather than
// straight to stdout) lets main.cpp capture the AST into a string for the
// --json output mode.
void printProgram(const Program& program, std::ostream& out);

const char* compareOpToString(CompareOp op);
