// pipeline.cpp
#include "pipeline.h"

#include <ostream>
#include <sstream>

#include "semantic.h"

extern int yyparse();
extern int lex_errors;
extern int yylineno;

namespace {

const char* kindName(OutputKind k) {
    switch (k) {
        case OutputKind::COUNT:   return "count";
        case OutputKind::DISPLAY: return "display";
        case OutputKind::DEFAULT: return "default";
    }
    return "default";
}

void emitStringArray(std::ostream& out, const std::vector<std::string>& items) {
    out << "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out << ",";
        out << "\"" << jsonEscape(items[i]) << "\"";
    }
    out << "]";
}

PipelineOutput fail(Stage stage, const std::string& message, PipelineOutput out) {
    out.ok = false;
    out.stage = stage;
    out.errors.push_back(message);
    return out;
}

} // namespace

const char* stageName(Stage s) {
    switch (s) {
        case Stage::Parse:    return "parse";
        case Stage::Semantic: return "semantic";
        case Stage::Runtime:  return "runtime";
        case Stage::Done:     return "done";
    }
    return "unknown";
}

int exitCodeFor(const PipelineOutput& out) {
    switch (out.stage) {
        case Stage::Parse:    return 2;
        case Stage::Semantic: return 3;
        case Stage::Runtime:  return 4;
        case Stage::Done:     return 0;
    }
    return 1;
}

void resetPipelineState() {
    g_program.clear();
    g_parseErrors.clear();
    lex_errors = 0;
    yylineno = 1;
}

PipelineOutput runPipeline(const ExecOptions& opts,
                           std::ostream* report,
                           const std::string& sourceLabel) {
    PipelineOutput out;

    // --- Phases 1 + 2: lexing and parsing ---
    if (report) {
        *report << "== Phase 1+2: Lexing & Parsing '" << sourceLabel << "' ==\n";
        report->flush();  // keep stdout/stderr interleaved in the right order
    }

    const int parseResult = yyparse();

    std::ostringstream astStream;
    printProgram(g_program, astStream);
    out.astText = astStream.str();
    out.statements = g_program.size();

    if (parseResult != 0 || lex_errors > 0) {
        out.stage = Stage::Parse;
        out.errors = g_parseErrors;
        if (out.errors.empty()) out.errors.push_back("Parsing failed.");
        if (report) *report << "\nParsing failed. See the error(s) above.\n";
        return out;
    }

    if (report) {
        *report << "Parsed successfully. " << out.statements << " statement(s) found.\n\n";
        *report << "== AST ==\n" << out.astText << "\n";
    }

    // --- Phase 3: semantic analysis ---
    if (!validateProgram(g_program, out.errors)) {
        out.stage = Stage::Semantic;
        if (report) {
            *report << "== Phase 3: Semantic Analysis ==\n";
            for (const auto& e : out.errors) *report << e << "\n";
            *report << "\nSemantic analysis failed; stopping before execution.\n";
        }
        return out;
    }

    if (report) {
        *report << "== Phase 3: Semantic Analysis ==\nNo semantic errors found.\n\n";
        *report << "== Phase 4: Query Execution ==\n";
    }

    // --- Phase 4: execution ---
    std::vector<ProteinRecord> records;

    for (const Statement& stmt : g_program) {
        if (stmt.type == StmtType::LOAD) {
            records.clear();
            std::string loadError;
            std::string label;
            bool loaded = false;

            if (stmt.loadSource == LoadSource::UNIPROT) {
                if (report) {
                    *report << "  [executor] Querying UniProt: \"" << stmt.uniprotQuery
                            << "\" (limit " << stmt.uniprotLimit << ")\n";
                    report->flush();
                }
                loaded = loadUniProt(stmt.uniprotQuery, stmt.uniprotLimit, opts,
                                     records, label, loadError);
            } else {
                if (report) {
                    *report << "  [executor] Reading dataset: " << stmt.loadFile << "\n";
                }
                label = stmt.loadFile;
                loaded = loadDataset(stmt.loadFile, records, loadError);
            }

            if (!loaded) {
                if (report) *report << loadError << "\n";
                return fail(Stage::Runtime, loadError, std::move(out));
            }

            out.sources.push_back(label + " -> " + std::to_string(records.size()) +
                                  " record(s)");
            if (report) {
                *report << "  [executor] Loaded " << records.size()
                        << " protein record(s) from " << label << ".\n";
            }
        } else {  // StmtType::FIND
            QueryResult result;
            std::string runError;
            if (!runQuery(stmt, records, result, runError)) {
                if (report) *report << runError << "\n";
                return fail(Stage::Runtime, runError, std::move(out));
            }
            out.results.push_back(result);
            if (report) printResult(result, *report);
        }
    }

    out.ok = true;
    out.stage = Stage::Done;
    if (report) *report << "\nDone.\n";
    return out;
}

std::string toJson(const PipelineOutput& out) {
    std::ostringstream os;
    os << "{";
    os << "\"ok\":" << (out.ok ? "true" : "false");
    os << ",\"stage\":\"" << stageName(out.stage) << "\"";
    os << ",\"statements\":" << out.statements;
    os << ",\"ast\":\"" << jsonEscape(out.astText) << "\"";
    os << ",\"errors\":";  emitStringArray(os, out.errors);
    os << ",\"sources\":"; emitStringArray(os, out.sources);

    os << ",\"results\":[";
    for (size_t r = 0; r < out.results.size(); ++r) {
        const QueryResult& res = out.results[r];
        if (r) os << ",";
        os << "{";
        os << "\"entity\":\"" << jsonEscape(res.entity) << "\"";
        os << ",\"kind\":\"" << kindName(res.kind) << "\"";
        os << ",\"matched\":" << res.matched;
        os << ",\"returned\":" << res.returned;
        os << ",\"columns\":"; emitStringArray(os, res.columns);
        os << ",\"rows\":[";
        for (size_t i = 0; i < res.rows.size(); ++i) {
            if (i) os << ",";
            emitStringArray(os, res.rows[i]);
        }
        os << "]}";
    }
    os << "]}";
    return os.str();
}
