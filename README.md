# ProteinDSL

A Domain-Specific Language (DSL) for Protein Database Query and Analysis
— a Compiler Design project applying lexical analysis, syntax analysis,
semantic analysis and query execution to a bioinformatics problem.

Queries run either against a bundled CSV or against **live UniProt data**.

**[Try it in your browser →](https://akhilteja2209.github.io/DSL-for-protein-database-query/)**
The compiler is built to WebAssembly, so the whole pipeline — lexer, parser,
semantic analyser, executor — runs in the tab. There is no backend.

## What it does

```
LOAD UNIPROT "kinase AND organism_id:9606 AND reviewed:true" TOP 100

FIND proteins
WHERE length > 800
SORT BY length DESC
TOP 10
DISPLAY proteinid name length function
```

```
LOAD "dataset/proteins.csv"

FIND proteins
WHERE organism = "Human" AND length > 500
OR organism = "Chicken"
SORT BY length DESC
DISPLAY proteinid name organism length
```

## System Architecture

```
 DSL Source Program
        |
        v
   Lexer (Flex)            Phase 1 - tokenizes keywords, identifiers,
        |                             operators, strings, numbers
        v
   Parser (Bison)          Phase 2 - validates tokens against the grammar,
        |                             builds an AST
        v
 Semantic Analyzer         Phase 3 - validates entity, fields, operators,
        |                             types, TOP, and LOAD-before-FIND
        v
  Execution Engine         Phase 4 - loads CSV or UniProt, filters, sorts,
        |                             selects columns, counts
        v
      Output               aligned table, COUNT, or one JSON document
```

## Tech Stack

| Concern                       | Choice                                        |
|-------------------------------|-----------------------------------------------|
| Lexical analysis              | Flex                                          |
| Syntax analysis / parsing     | GNU Bison (`bison -d`, C++ output)            |
| Semantic analysis & execution | C++17                                         |
| Data sources                  | CSV (`dataset/proteins.csv`), UniProt REST API |
| HTTP client                   | `curl` natively, `fetch()` in the browser     |
| Browser build                 | Emscripten (`em++`) → WebAssembly + ASYNCIFY  |
| Web playground                | Python 3 standard library only                |
| Build                         | GNU Make + g++ (+ `emcc` for `make wasm`)     |
| Target environment            | Linux / WSL / Windows / any modern browser    |

The executor shells out to `curl` rather than linking libcurl, so the
compiler itself has no third-party dependencies and builds anywhere flex,
bison and a C++17 compiler are present.

## Language Reference

Protein attributes: `proteinid`, `name`, `organism`, `length`, `function`,
`sequence`. Attribute names are case-insensitive; keywords are uppercase.

| Clause | Meaning |
|--------|---------|
| `LOAD "path.csv"` | Read a local CSV (`ProteinID,Name,Organism,Length,Function,Sequence`). |
| `LOAD UNIPROT "<query>" [TOP n]` | Run a live [UniProt](https://rest.uniprot.org) search. `n` defaults to 50, max 500. |
| `FIND proteins` | Query the loaded records. Every clause below is optional. |
| `SEARCH "text"` | Case-insensitive substring scan across **every** attribute, sequences included — which makes it a naive motif search. |
| `WHERE <cond> [AND\|OR <cond>]...` | Filter. `AND` binds tighter than `OR`. |
| `SORT BY <field> [ASC\|DESC]` | Ordering; numeric for `length`, case-insensitive text otherwise. `ASC` is the default. |
| `TOP n` | Keep only the first `n` results. |
| `DISPLAY <fields>` | Choose output columns. |
| `COUNT` | Report the number of matches instead of listing them. |

Comparison operators are `=`, `!=`, `>`, `<`, `>=`, `<=`. The ordering
operators are only valid on `length`; the others are valid on any
attribute.

Two semantics worth knowing:

- **`=` on a text attribute is an exact, case-insensitive match**, not a
  substring match. UniProt reports organisms as `Homo sapiens (Human)`, so
  filter species in the UniProt query itself (`organism_id:9606`) or use
  `SEARCH` for partial text.
- **UniProt function text is cleaned**: the leading `FUNCTION: ` and the
  `{ECO:...}` evidence tags are stripped, since they are metadata rather
  than content. Every other field is passed through verbatim.

## Project Structure

```
ProteinDSL/
│
├── lexer.l               Phase 1: Flex lexer
├── parser.y              Phase 2: Bison grammar
├── ast.h / ast.cpp       Internal representation (AST) + pretty-printer
├── semantic.h / .cpp     Phase 3: semantic checks
├── executor.h / .cpp     Phase 4: CSV + UniProt loading, filtering, sorting
├── pipeline.h / .cpp     The four phases wired together, shared by both builds
├── main.cpp              Front end 1: the native CLI
├── wasm_api.cpp          Front end 2: the WebAssembly entry point
├── Makefile
│
├── dataset/
│   └── proteins.csv      Sample protein records
│
├── sample_queries/
│   ├── valid_query.dsl     The basic end-to-end example
│   ├── advanced_query.dsl  OR / SORT BY / TOP / SEARCH / COUNT
│   ├── uniprot_query.dsl   Live UniProt search
│   └── invalid_query.dsl   Demonstrates semantic error reporting
│
├── web/                  The page: index.html, app.css, app.js (one source,
│                         used by both the local server and the Pages build)
├── server/
│   └── app.py            Local playground (Python stdlib only)
├── tools/
│   └── build_pages.py    Assembles docs/ from web/ + sample_queries/
└── docs/                 Built by `make wasm`; what GitHub Pages serves
```

`main.cpp` and `wasm_api.cpp` are two front ends onto the same
`pipeline.cpp`: one points Flex at a `FILE*`, the other at a JavaScript
string. Every phase after that is identical code, which is why the browser
and the CLI give the same answers.

## Building & Running

Requires `flex`, `bison`, and a C++17 compiler (`g++`).

```bash
# Debian/Ubuntu
sudo apt-get install -y flex bison g++ make
```

```powershell
# Windows, via Scoop
scoop install gcc winflexbison make
```

```bash
make
./bin/proteindsl sample_queries/valid_query.dsl
./bin/proteindsl sample_queries/advanced_query.dsl
./bin/proteindsl sample_queries/uniprot_query.dsl
./bin/proteindsl sample_queries/invalid_query.dsl
```

`make run-valid` and `make run-invalid` do the same thing.

### Command-line options

```
bin/proteindsl [options] <path-to-.dsl-file | ->
```

| Option | Effect |
|--------|--------|
| `--json` | Emit one JSON document (AST, errors, results) instead of the phase report. |
| `--dataset-root <dir>` | Confine file `LOAD`s to `<dir>`. |
| `--no-network` | Refuse `LOAD UNIPROT`. |
| `--cache-dir <dir>` | Where fetched UniProt responses are kept (default `.cache/`). |

A path of `-` reads the program from stdin.

### Expected output (valid query)

```
== Phase 1+2: Lexing & Parsing 'sample_queries/valid_query.dsl' ==
Parsed successfully. 2 statement(s) found.

== AST ==
Statement 1: LOAD
  file: "dataset/proteins.csv"
Statement 2: FIND
  entity: proteins
  WHERE organism = "Human"
    AND length > 300
  DISPLAY name length function

== Phase 3: Semantic Analysis ==
No semantic errors found.

== Phase 4: Query Execution ==
  [executor] Reading dataset: dataset/proteins.csv
  [executor] Loaded 5 protein record(s) from dataset/proteins.csv.
  +------------+--------+------------------+
  | name       | length | function         |
  +------------+--------+------------------+
  | Hemoglobin | 574    | Oxygen Transport |
  | Albumin    | 609    | Transport        |
  +------------+--------+------------------+
  2 row(s)

Done.
```

### Expected output (invalid query)

```
== Phase 3: Semantic Analysis ==
Semantic Error: Unknown protein attribute 'unknown_field'
Semantic Error: Cannot compare string attribute 'name' with numeric operator '>'
Semantic Error: Attribute 'name' expects a string value, got 500

Semantic analysis failed; stopping before execution.
```

## Web Playground

The same page ([`web/`](web)) runs against either backend, and `app.js` picks
whichever is present:

| | Backend | Where |
|---|---|---|
| **Local** | `server/app.py` runs the native binary | `http://127.0.0.1:8000` |
| **Pages** | the WebAssembly module runs in the tab | the live link above |

```bash
make                       # the server runs the compiled binary
python server/app.py       # http://127.0.0.1:8000  (or: make serve)
```

A textarea for the program, the sample queries as one-click examples, the
results as tables, and the parsed AST alongside them. `Ctrl+Enter` runs.

The server contains no query logic — it pipes the submitted program into
`bin/proteindsl -` with `--json` and renders what comes back.

### On running submitted programs

A submitted program is arbitrary input, so the server runs the binary with
`--dataset-root dataset`, which rejects any `LOAD` resolving outside that
directory, under a 90-second timeout and an 8 KB program limit. It binds to
`127.0.0.1` by default. `LOAD UNIPROT` stays enabled because the executor
builds the UniProt URL itself and percent-encodes the query, so a query
string cannot become shell syntax or reach another host.

That is enough for local use. It is **not** an audited sandbox — don't put
this on a public address without putting it in a container first. The
GitHub Pages build sidesteps the question entirely: there is no server, and
each visitor's browser only ever runs their own program in their own tab.

## Deploying to GitHub Pages

GitHub Pages serves static files, so the Python server cannot run there.
Instead the compiler itself is built to WebAssembly and the whole pipeline
runs client-side — the same approach `onnxruntime-web` uses to run a model
in a browser.

```bash
# One-time: install Emscripten
git clone https://github.com/emscripten-core/emsdk && cd emsdk
./emsdk install latest && ./emsdk activate latest && source ./emsdk_env.sh
```

```bash
make wasm      # -> docs/  (~370 KB of wasm)
python -m http.server -d docs 8080     # check it locally first
```

Then point Pages at **branch `main`, folder `/docs`**. `docs/` is committed
because deploy-from-branch has nothing to build with.

How the browser build differs, all of it inside `#ifdef __EMSCRIPTEN__`:

- `LOAD UNIPROT` calls the page's `fetch()` instead of shelling out to
  `curl`. ASYNCIFY suspends the wasm stack across the `await`, so the
  executor keeps its ordinary synchronous shape. UniProt sends
  `Access-Control-Allow-Origin: *`, so no proxy is needed.
- Responses are cached in memory for the life of the tab rather than in
  `.cache/`.
- `dataset/` is preloaded into the virtual filesystem, so
  `LOAD "dataset/proteins.csv"` works unchanged. Nothing else exists in that
  filesystem, so there is no dataset root to enforce.

## Current Status

Implemented and tested end to end:

- **Lexer** — every keyword, operator and literal in the language, with
  lexical-error reporting by line number.
- **Parser** — the full grammar above, conflict-free, building an AST.
- **Semantic analysis** — unknown entity, unknown attribute, operator/type
  mismatch in both directions, unknown `SORT BY` field, non-positive `TOP`,
  and `FIND` before any `LOAD`.
- **Execution** — CSV and live UniProt loading, `SEARCH`, `WHERE` with
  `AND`/`OR`, `SORT BY` with direction, `TOP`, `COUNT`, and `DISPLAY`
  column selection, rendered as an aligned table or as JSON.
- **Web playground** — runs all of the above in a browser, either against the
  local server or entirely client-side as WebAssembly. Both paths were checked
  to return identical results for the same programs.

Known limits:

- `WHERE` has no parentheses; precedence is fixed at `AND` over `OR`.
- Ordering comparisons are only meaningful on `length` (the one numeric
  attribute), which semantic analysis enforces.
- One `LOAD` is in effect at a time — a later `LOAD` replaces the dataset
  rather than joining it.
- UniProt responses are cached by URL and never expire; delete `.cache/`
  to force a refetch.

## License

For academic/coursework use.
