// wasm_api.cpp
// -----------------------------------------------------------------------
// WebAssembly entry point, built only by `make wasm`.
//
// This is the browser's equivalent of main.cpp: it points the Flex lexer at a
// JavaScript string instead of a FILE*, runs the same pipeline.cpp phases the
// native CLI runs, and returns the same JSON document server/app.py would have
// produced. That is what lets the whole compiler run on GitHub Pages with no
// backend at all.
//
// The call is asynchronous from JavaScript's side because LOAD UNIPROT awaits
// a fetch (see js_fetch_text in executor.cpp), so callers use
// Module.ccall(..., { async: true }) and get a Promise.
// -----------------------------------------------------------------------
#include <emscripten.h>

#include <string>

#include "executor.h"
#include "pipeline.h"

// Flex's string-buffer interface: the browser has no stdin to read from.
struct yy_buffer_state;
typedef yy_buffer_state* YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char* str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);

extern "C" {

// Compiles and runs one program, returning the JSON document as a UTF-8
// string. The buffer belongs to this module and stays valid until the next
// call, so JavaScript must not free it.
EMSCRIPTEN_KEEPALIVE
const char* proteindsl_run_json(const char* source) {
    static std::string output;

    ExecOptions opts;
    // In the browser the only files that exist are the ones preloaded into the
    // virtual filesystem (dataset/), so there is nothing for a file LOAD to
    // escape to and no dataset root to enforce.
    opts.datasetRoot.clear();
    opts.allowNetwork = true;

    resetPipelineState();

    YY_BUFFER_STATE buffer = yy_scan_string(source ? source : "");
    // No report stream: the page renders the JSON itself.
    const PipelineOutput out = runPipeline(opts, nullptr, "<browser>");
    yy_delete_buffer(buffer);

    output = toJson(out);
    return output.c_str();
}

}  // extern "C"
