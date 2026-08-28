// main.cpp
// -----------------------------------------------------------------------
// ProteinDSL driver.
//
// Runs the pipeline from the System Architecture slide (slide 8) as far
// as it is currently implemented:
//
//   DSL source --Lexer(Flex)--> tokens --Parser(Bison)--> AST
//              --Semantic Analyzer--> validated AST
//              --Execution Engine (CSV load only, see executor.h)--> output
//
// Usage: bin/proteindsl <path-to-.dsl-file>
// -----------------------------------------------------------------------
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "ast.h"
#include "semantic.h"
#include "executor.h"

extern FILE* yyin;
extern int yyparse();

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path-to-.dsl-file>\n";
        return 1;
    }

    yyin = std::fopen(argv[1], "r");
    if (!yyin) {
        std::cerr << "Runtime Error: could not open DSL source file '" << argv[1] << "'\n";
        return 1;
    }

    std::cout << "== Phase 1+2: Lexing & Parsing '" << argv[1] << "' ==\n";
    std::cout.flush(); // keep stdout/stderr interleaved in the right order
    int parseResult = yyparse();
    std::fclose(yyin);

    if (parseResult != 0) {
        std::cerr << "Parsing failed. See syntax error(s) above.\n";
        return 1;
    }
    std::cout << "Parsed successfully. " << g_program.size() << " statement(s) found.\n\n";

    std::cout << "== AST ==\n";
    printProgram(g_program);
    std::cout << "\n";

    std::cout << "== Phase 3: Semantic Analysis ==\n";
    std::vector<std::string> errors;
    bool ok = validateProgram(g_program, errors);
    if (!ok) {
        for (const auto& e : errors) std::cerr << e << "\n";
        std::cerr << "\nSemantic analysis failed; stopping before execution.\n";
        return 1;
    }
    std::cout << "No semantic errors found.\n\n";

    std::cout << "== Phase 4: Query Execution ==\n";
    std::vector<ProteinRecord> records;
    std::string loadError;
    bool datasetLoaded = false;

    for (const Statement& stmt : g_program) {
        if (stmt.type == StmtType::LOAD) {
            std::cout << "  [executor] Reading dataset: " << stmt.loadFile << "\n";
            records.clear();
            if (!loadDataset(stmt.loadFile, records, loadError)) {
                std::cerr << loadError << "\n";
                return 1;
            }
            datasetLoaded = true;
            std::cout << "  [executor] Loaded " << records.size() << " protein record(s).\n";
        } else if (stmt.type == StmtType::FIND) {
            if (!datasetLoaded) {
                std::cerr << "Runtime Error: FIND used before LOAD; no dataset in memory.\n";
                return 1;
            }
            runQuery(stmt, records);
        }
    }

    std::cout << "\nDone. (Filtering, sorting, counting and result display are the next\n"
                 "implementation milestone -- see README \"Current Status\".)\n";
    return 0;
}
