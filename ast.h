// ast.h
// -----------------------------------------------------------------------
// Internal representation for parsed ProteinDSL programs.
//
// Pipeline stage: Parser (Bison) -> AST -> [Semantic Analyzer] -> [Executor]
// See System Architecture, slide 8 of the project plan.
//
// Status: FIND / WHERE (AND only) / DISPLAY are represented and populated
// by the parser. OR, COUNT, SORT BY and TOP are recognized by the lexer
// (see lexer.l) but not yet represented here — that is left for the next
// implementation step (see README "Current Status").
// -----------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>
#include <iosfwd>

enum class StmtType { LOAD, FIND };

enum class CompareOp { EQ, GT, LT, GE, LE, NE };

// A single WHERE condition, e.g. `length > 300` or `organism = "Human"`.
struct Condition {
    std::string field;
    CompareOp   op;
    std::string value;      // raw literal text; typed at semantic-analysis time
    bool        isNumeric;  // true if the literal was a NUMBER token
};

// One DSL statement. Only the fields relevant to `type` are populated.
struct Statement {
    StmtType type;

    // LOAD "file.csv"
    std::string loadFile;

    // FIND <entity> [WHERE cond (AND cond)*] [DISPLAY field...]
    std::string entity;
    std::vector<Condition>  conditions;      // combined with AND (OR: TODO)
    std::vector<std::string> displayFields;  // empty => display all columns
};

using Program = std::vector<Statement>;

// Built up by the Bison parser as it reduces the input; main.cpp reads this
// once yyparse() returns 0 (success).
extern Program g_program;

// Pretty-prints the parsed program to stdout. Used by main.cpp to give a
// visible "Show successful parsing / AST" step (Expected Demonstration,
// slide 18, item 4).
void printProgram(const Program& program);

const char* compareOpToString(CompareOp op);
