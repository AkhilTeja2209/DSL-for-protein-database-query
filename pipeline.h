// pipeline.h
// -----------------------------------------------------------------------
// The four compiler phases, wired together once and shared by both front
// ends: the native CLI (main.cpp) and the WebAssembly entry point
// (wasm_api.cpp) that the GitHub Pages build calls from the browser.
//
// The caller is responsible for pointing the lexer at its input first --
// `yyin = file` for the CLI, `yy_scan_string(...)` for wasm -- because that
// is the only part of the pipeline that differs between them.
// -----------------------------------------------------------------------
#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "ast.h"
#include "executor.h"

// Where the pipeline stopped. Reported in JSON so a caller can tell a typo in
// the query apart from UniProt being unreachable.
enum class Stage { Parse, Semantic, Runtime, Done };

const char* stageName(Stage s);

struct PipelineOutput {
    bool                     ok = false;
    Stage                    stage = Stage::Parse;
    size_t                   statements = 0;
    std::string              astText;
    std::vector<std::string> errors;
    std::vector<std::string> sources;   // one per LOAD, with record counts
    std::vector<QueryResult> results;   // one per FIND
};

// Clears the parser's global state. The CLI runs one program per process, but
// the wasm module runs many per page load, so this must be called first.
void resetPipelineState();

// Runs phases 1-4. When `report` is non-null the human-readable phase report
// is written to it as the run progresses; when it is null the run is silent
// and the caller renders `PipelineOutput` itself (see toJson).
//
// `sourceLabel` only names the input in the report header.
PipelineOutput runPipeline(const ExecOptions& opts,
                           std::ostream* report,
                           const std::string& sourceLabel);

// Renders a completed run as one JSON document.
std::string toJson(const PipelineOutput& out);

// Process exit code matching the phase that failed: 0 done, 2 parse,
// 3 semantic, 4 runtime.
int exitCodeFor(const PipelineOutput& out);
