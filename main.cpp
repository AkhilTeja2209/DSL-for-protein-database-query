// main.cpp
// -----------------------------------------------------------------------
// ProteinDSL driver.
//
// Runs the full pipeline from the System Architecture section of the README:
//
//   DSL source --Lexer(Flex)--> tokens --Parser(Bison)--> AST
//              --Semantic Analyzer--> validated AST
//              --Execution Engine--> results
//
// Usage: bin/proteindsl [options] <path-to-.dsl-file | ->
//
//   --json                 emit one JSON document instead of the phase report
//   --dataset-root <dir>   confine file LOADs to <dir> (used by the web server)
//   --no-network           refuse LOAD UNIPROT
//   --cache-dir <dir>      where fetched UniProt responses are kept
//
// A path of "-" reads the program from stdin, which is how server/app.py
// hands user-submitted programs to the binary without touching the disk.
// -----------------------------------------------------------------------
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ast.h"
#include "semantic.h"
#include "executor.h"

extern FILE* yyin;
extern int yyparse();
extern int lex_errors;

namespace {

struct Options {
    std::string sourcePath;
    bool        json = false;
    ExecOptions exec;
};

// Where the pipeline stopped. Reported in --json so a caller can tell a typo
// in the query apart from UniProt being unreachable.
enum class Stage { Parse, Semantic, Runtime, Done };

const char* stageName(Stage s) {
    switch (s) {
        case Stage::Parse:    return "parse";
        case Stage::Semantic: return "semantic";
        case Stage::Runtime:  return "runtime";
        case Stage::Done:     return "done";
    }
    return "unknown";
}

const char* kindName(OutputKind k) {
    switch (k) {
        case OutputKind::COUNT:   return "count";
        case OutputKind::DISPLAY: return "display";
        case OutputKind::DEFAULT: return "default";
    }
    return "default";
}

void usage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0 << " [options] <path-to-.dsl-file | ->\n"
        << "\n"
        << "  --json                 emit one JSON document instead of the phase report\n"
        << "  --dataset-root <dir>   confine file LOADs to <dir>\n"
        << "  --no-network           refuse LOAD UNIPROT\n"
        << "  --cache-dir <dir>      where fetched UniProt responses are kept\n";
}

bool parseArgs(int argc, char** argv, Options& opts) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--json") {
            opts.json = true;
        } else if (arg == "--no-network") {
            opts.exec.allowNetwork = false;
        } else if (arg == "--dataset-root") {
            if (i + 1 >= argc) return false;
            opts.exec.datasetRoot = argv[++i];
        } else if (arg == "--cache-dir") {
            if (i + 1 >= argc) return false;
            opts.exec.cacheDir = argv[++i];
        } else if (!arg.empty() && arg[0] == '-' && arg != "-") {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        } else if (opts.sourcePath.empty()) {
            opts.sourcePath = arg;
        } else {
            return false;  // more than one input file
        }
    }
    return !opts.sourcePath.empty();
}

// --- JSON emission ------------------------------------------------------

void emitStringArray(std::ostream& out, const std::vector<std::string>& items) {
    out << "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out << ",";
        out << "\"" << jsonEscape(items[i]) << "\"";
    }
    out << "]";
}

void emitJson(std::ostream& out,
              Stage stage,
              bool ok,
              size_t statementCount,
              const std::string& astText,
              const std::vector<std::string>& errors,
              const std::vector<std::string>& sources,
              const std::vector<QueryResult>& results) {
    out << "{";
    out << "\"ok\":" << (ok ? "true" : "false");
    out << ",\"stage\":\"" << stageName(stage) << "\"";
    out << ",\"statements\":" << statementCount;
    out << ",\"ast\":\"" << jsonEscape(astText) << "\"";
    out << ",\"errors\":";  emitStringArray(out, errors);
    out << ",\"sources\":"; emitStringArray(out, sources);

    out << ",\"results\":[";
    for (size_t r = 0; r < results.size(); ++r) {
        const QueryResult& res = results[r];
        if (r) out << ",";
        out << "{";
        out << "\"entity\":\"" << jsonEscape(res.entity) << "\"";
        out << ",\"kind\":\"" << kindName(res.kind) << "\"";
        out << ",\"matched\":" << res.matched;
        out << ",\"returned\":" << res.returned;
        out << ",\"columns\":"; emitStringArray(out, res.columns);
        out << ",\"rows\":[";
        for (size_t i = 0; i < res.rows.size(); ++i) {
            if (i) out << ",";
            emitStringArray(out, res.rows[i]);
        }
        out << "]}";
    }
    out << "]}";
    out << "\n";
}

} // namespace

int main(int argc, char** argv) {
    Options opts;
    if (!parseArgs(argc, argv, opts)) {
        usage(argv[0]);
        return 1;
    }

    const bool fromStdin = (opts.sourcePath == "-");
    if (fromStdin) {
        yyin = stdin;
    } else {
        yyin = std::fopen(opts.sourcePath.c_str(), "r");
        if (!yyin) {
            const std::string msg = "Runtime Error: could not open DSL source file '" +
                                    opts.sourcePath + "'";
            if (opts.json) {
                emitJson(std::cout, Stage::Runtime, false, 0, "", {msg}, {}, {});
            } else {
                std::cerr << msg << "\n";
            }
            return 1;
        }
    }

    // --- Phases 1 + 2: lexing and parsing ---
    if (!opts.json) {
        std::cout << "== Phase 1+2: Lexing & Parsing '" << opts.sourcePath << "' ==\n";
        std::cout.flush();  // keep stdout/stderr interleaved in the right order
    }

    const int parseResult = yyparse();
    if (!fromStdin) std::fclose(yyin);

    std::ostringstream astStream;
    printProgram(g_program, astStream);
    const std::string astText = astStream.str();

    if (parseResult != 0 || lex_errors > 0) {
        std::vector<std::string> errors = g_parseErrors;
        if (errors.empty()) errors.push_back("Parsing failed.");
        if (opts.json) {
            emitJson(std::cout, Stage::Parse, false, g_program.size(), astText,
                     errors, {}, {});
        } else {
            std::cerr << "\nParsing failed. See the error(s) above.\n";
        }
        return 2;
    }

    if (!opts.json) {
        std::cout << "Parsed successfully. " << g_program.size()
                  << " statement(s) found.\n\n";
        std::cout << "== AST ==\n" << astText << "\n";
    }

    // --- Phase 3: semantic analysis ---
    std::vector<std::string> errors;
    if (!validateProgram(g_program, errors)) {
        if (opts.json) {
            emitJson(std::cout, Stage::Semantic, false, g_program.size(), astText,
                     errors, {}, {});
        } else {
            std::cout << "== Phase 3: Semantic Analysis ==\n";
            for (const auto& e : errors) std::cerr << e << "\n";
            std::cerr << "\nSemantic analysis failed; stopping before execution.\n";
        }
        return 3;
    }

    if (!opts.json) {
        std::cout << "== Phase 3: Semantic Analysis ==\nNo semantic errors found.\n\n";
        std::cout << "== Phase 4: Query Execution ==\n";
    }

    // --- Phase 4: execution ---
    std::vector<ProteinRecord> records;
    std::vector<std::string>   sources;
    std::vector<QueryResult>   results;

    for (const Statement& stmt : g_program) {
        if (stmt.type == StmtType::LOAD) {
            records.clear();
            std::string loadError;
            std::string label;

            bool loaded = false;
            if (stmt.loadSource == LoadSource::UNIPROT) {
                if (!opts.json) {
                    std::cout << "  [executor] Querying UniProt: \""
                              << stmt.uniprotQuery << "\" (limit "
                              << stmt.uniprotLimit << ")\n";
                    std::cout.flush();
                }
                loaded = loadUniProt(stmt.uniprotQuery, stmt.uniprotLimit, opts.exec,
                                     records, label, loadError);
            } else {
                if (!opts.json) {
                    std::cout << "  [executor] Reading dataset: " << stmt.loadFile << "\n";
                }
                label = stmt.loadFile;
                loaded = loadDataset(stmt.loadFile, opts.exec, records, loadError);
            }

            if (!loaded) {
                if (opts.json) {
                    emitJson(std::cout, Stage::Runtime, false, g_program.size(), astText,
                             {loadError}, sources, results);
                } else {
                    std::cerr << loadError << "\n";
                }
                return 4;
            }

            sources.push_back(label + " -> " + std::to_string(records.size()) +
                              " record(s)");
            if (!opts.json) {
                std::cout << "  [executor] Loaded " << records.size()
                          << " protein record(s) from " << label << ".\n";
            }
        } else {  // StmtType::FIND
            QueryResult result;
            std::string runError;
            if (!runQuery(stmt, records, result, runError)) {
                if (opts.json) {
                    emitJson(std::cout, Stage::Runtime, false, g_program.size(), astText,
                             {runError}, sources, results);
                } else {
                    std::cerr << runError << "\n";
                }
                return 4;
            }
            results.push_back(result);
            if (!opts.json) printResult(result, std::cout);
        }
    }

    if (opts.json) {
        emitJson(std::cout, Stage::Done, true, g_program.size(), astText,
                 {}, sources, results);
    } else {
        std::cout << "\nDone.\n";
    }
    return 0;
}
