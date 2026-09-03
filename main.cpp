// main.cpp
// -----------------------------------------------------------------------
// ProteinDSL command-line driver.
//
// The four phases themselves live in pipeline.cpp, shared with the
// WebAssembly build (wasm_api.cpp). This file only parses arguments, points
// the lexer at a file or stdin, and chooses between the human-readable phase
// report and the JSON document.
//
// Usage: bin/proteindsl [options] <path-to-.dsl-file | ->
//
//   --json                 emit one JSON document instead of the phase report
//   --cache-dir <dir>      where fetched UniProt responses are kept
//
// A path of "-" reads the program from stdin.
// -----------------------------------------------------------------------
#include <cstdio>
#include <iostream>
#include <string>

#include "executor.h"
#include "pipeline.h"

extern FILE* yyin;

namespace {

struct Options {
    std::string sourcePath;
    bool        json = false;
    ExecOptions exec;
};

void usage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0 << " [options] <path-to-.dsl-file | ->\n"
        << "\n"
        << "  --json                 emit one JSON document instead of the phase report\n"
        << "  --cache-dir <dir>      where fetched UniProt responses are kept\n";
}

bool parseArgs(int argc, char** argv, Options& opts) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--json") {
            opts.json = true;
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
                PipelineOutput out;
                out.stage = Stage::Runtime;
                out.errors.push_back(msg);
                std::cout << toJson(out) << "\n";
            } else {
                std::cerr << msg << "\n";
            }
            return 1;
        }
    }

    resetPipelineState();

    // The phase report goes to stdout as it happens; --json stays silent and
    // prints one document at the end instead.
    const PipelineOutput out =
        runPipeline(opts.exec, opts.json ? nullptr : &std::cout, opts.sourcePath);

    if (!fromStdin) std::fclose(yyin);

    if (opts.json) std::cout << toJson(out) << "\n";
    return exitCodeFor(out);
}
