# ProteinDSL

**A Domain-Specific Language for Protein Database Query and Analysis**
— a Compiler Design project applying lexical analysis, syntax analysis,
semantic analysis and query execution to a bioinformatics problem.

**[▶ Try it live](https://akhilteja2209.github.io/DSL-for-protein-database-query/)** — the
compiler is built to WebAssembly, so the whole pipeline runs in your browser.
There is no backend.

```
LOAD UNIPROT "kinase AND organism_id:9606 AND reviewed:true" TOP 100

FIND proteins
WHERE length > 800
SORT BY length DESC
TOP 10
DISPLAY proteinid name length function
```

---

## Contents

1. [What this is](#1-what-this-is)
2. [Quick start](#2-quick-start)
3. [Language documentation](#3-language-documentation)
4. [Architecture](#4-architecture)
5. [Building and running](#5-building-and-running)
6. [The web playground](#6-the-web-playground)
7. [Deployment](#7-deployment)
8. [Design decisions and limits](#8-design-decisions-and-limits)
9. [Repository layout](#9-repository-layout)

---

## 1. What this is

Biologists querying protein databases either click through web forms or write
SQL against a local copy. Neither is expressive *and* convenient: forms cannot
express compound conditions, and SQL requires knowing a schema that has nothing
to do with proteins.

ProteinDSL is a small language in the vocabulary of the domain — `FIND
proteins WHERE organism = "Human" AND length > 300` — implemented as a real
compiler with all four classical phases. A query is lexed, parsed into an
abstract syntax tree, checked for meaning, and then executed against either a
local CSV or a live [UniProt](https://www.uniprot.org/) search.

The point of the project is the **compiler**, not the database. Every phase is
visible and separately inspectable: the CLI prints the token stream's outcome,
the AST, the semantic verdict and then the results, in that order.

---

## 2. Quick start

**In a browser — nothing to install.** Open the
[live site](https://akhilteja2209.github.io/DSL-for-protein-database-query/),
click an example, press **Run** (or `Ctrl`+`Enter`).

**Natively:**

```bash
# Debian / Ubuntu / WSL
sudo apt-get install -y flex bison g++ make
```
```powershell
# Windows, via Scoop
scoop install gcc winflexbison make
```
```bash
make
./bin/proteindsl sample_queries/valid_query.dsl
```

**The playground locally:**

```bash
make serve      # builds the wasm site, serves it on http://localhost:8080
```

---

## 3. Language documentation

### 3.1 The data model

Every query operates on one entity, `proteins`, whose records have six
attributes:

| Attribute | Type | Notes |
|-----------|------|-------|
| `proteinid` | string | Accession, e.g. `P01308` |
| `name` | string | Protein name |
| `organism` | string | Source organism |
| `length` | **number** | Residue count — the only numeric attribute |
| `function` | string | Free-text functional annotation |
| `sequence` | string | Amino-acid sequence |

Attribute names are **case-insensitive** (`Length`, `length` and `LENGTH` are
the same attribute). Keywords are **uppercase only** — `find` is an
identifier, not the `FIND` keyword.

### 3.2 Lexical structure

| Token class | Rule | Examples |
|-------------|------|----------|
| Keyword | Fixed uppercase words | `LOAD` `UNIPROT` `FIND` `WHERE` `AND` `OR` `SEARCH` `SORT` `BY` `ASC` `DESC` `TOP` `DISPLAY` `COUNT` |
| Operator | Two-character forms matched first | `>=` `<=` `!=` `=` `>` `<` |
| Identifier | `[A-Za-z_][A-Za-z0-9_]*` | `proteins` `length` `organism` |
| String | `"` then any characters except `"` then `"` | `"Human"` `"dataset/proteins.csv"` |
| Number | `[0-9]+(\.[0-9]+)?` | `300` `1.5` |
| Comment | `//` to end of line | `// live UniProt search` |
| Whitespace | Spaces, tabs, newlines — insignificant | |

A string literal **cannot contain a double quote**; there is no escape
sequence. Newlines are not significant, so a statement may be written across
as many lines as is readable.

### 3.3 Grammar

```ebnf
program     ::= statement*

statement   ::= "LOAD" string
              | "LOAD" "UNIPROT" string top_opt
              | "FIND" identifier search_opt where_opt sort_opt top_opt output_opt

search_opt  ::= ε | "SEARCH" string
where_opt   ::= ε | "WHERE" or_expr

or_expr     ::= and_expr
              | or_expr "OR" and_expr
and_expr    ::= condition
              | and_expr "AND" condition
condition   ::= identifier cmp_op value

cmp_op      ::= "=" | "!=" | ">" | "<" | ">=" | "<="
value       ::= string | number

sort_opt    ::= ε | "SORT" "BY" identifier dir_opt
dir_opt     ::= ε | "ASC" | "DESC"
top_opt     ::= ε | "TOP" number
output_opt  ::= ε | "DISPLAY" id_list | "COUNT"
id_list     ::= identifier | id_list identifier
```

The grammar is **LALR(1) and conflict-free** — `bison -Wall` reports no
shift/reduce or reduce/reduce conflicts. Every optional clause is introduced
by a distinct keyword, which is what keeps it unambiguous.

### 3.4 Statements

#### `LOAD "path.csv"`

Reads a local CSV into memory. The file must have a header row and six
columns in this order:

```csv
ProteinID,Name,Organism,Length,Function,Sequence
P001,Hemoglobin,Human,574,Oxygen Transport,MVLSPADKTNVKAAWGKV...
```

Fields may be quoted RFC-4180 style (`"a,b"`, with `""` for a literal quote).
`Length` must parse as an integer.

#### `LOAD UNIPROT "<query>" [TOP n]`

Runs a live search against the UniProt REST API and loads the results. The
query string is UniProt's own syntax, not ProteinDSL's:

```
LOAD UNIPROT "insulin AND organism_id:9606 AND reviewed:true" TOP 50
```

`TOP n` sets how many entries to fetch — default **50**, maximum **500** (the
API's per-page cap). Responses are cached, so re-running a query costs nothing.

#### `FIND proteins [clauses...]`

Queries whatever the most recent `LOAD` put in memory. Every clause is
optional, but they must appear in this order:

```
FIND proteins
  SEARCH "text"
  WHERE <conditions>
  SORT BY <field> [ASC|DESC]
  TOP <n>
  DISPLAY <fields> | COUNT
```

A program may contain several `LOAD`s and several `FIND`s. Each `LOAD`
**replaces** the dataset; each `FIND` reports separately.

### 3.5 Clauses

**`SEARCH "text"`** — a case-insensitive substring scan across *every*
attribute, including `sequence`. Because it searches the sequence, it doubles
as a naive motif search:

```
FIND proteins SEARCH "MVLSPADK" DISPLAY name length
```

**`WHERE <conditions>`** — `AND` binds tighter than `OR`, as in most
languages. There are no parentheses, so:

```
WHERE organism = "Human" AND length > 500 OR organism = "Chicken"
```

means `(organism = "Human" AND length > 500) OR (organism = "Chicken")`.
Internally the clause is stored in **disjunctive normal form** — a list of
AND-groups, any one of which may match.

**`SORT BY <field> [ASC|DESC]`** — `ASC` is the default. `length` sorts
numerically; every other attribute sorts as case-insensitive text. The sort is
*stable*, so equal keys keep their input order.

**`TOP n`** — keeps the first `n` results after sorting. `n` must be a
positive integer.

**`DISPLAY <fields>`** — chooses the output columns, in the order given.
Without it, all six attributes are shown.

**`COUNT`** — reports the number of results instead of listing them. Mutually
exclusive with `DISPLAY` (the grammar allows only one).

### 3.6 Type rules

`length` is numeric; the other five attributes are strings. This drives every
type check:

| Comparison | Valid on | Meaning |
|------------|----------|---------|
| `=` `!=` | any attribute | On strings: **case-insensitive exact match** — not a substring |
| `>` `<` `>=` `<=` | `length` only | Numeric comparison |

A numeric literal may only be compared against `length`, and `length` may only
be compared against a numeric literal. Both directions are checked, so
`length = "300"` and `name = 300` are each errors. This is possible because
the parser records *which token the lexer actually saw* rather than guessing
from the text.

> **`=` is exact, not partial.** UniProt reports organisms as
> `Homo sapiens (Human)`, so `organism = "Human"` will not match UniProt data.
> Filter the species in the UniProt query itself (`organism_id:9606`), or use
> `SEARCH "Human"` for partial text.

### 3.7 Execution order

For each `FIND`, in this order:

1. **Filter** — a record must pass `SEARCH` *and* `WHERE`.
2. **Sort** — by `SORT BY`, if present.
3. **Truncate** — to `TOP n`, if present.
4. **Project** — select `DISPLAY` columns, or count.

The distinction matters for `COUNT`: it reports the size of the result set
*after* `TOP`. The text and JSON output also report the pre-`TOP` figure, as
`N row(s) of M matched`.

### 3.8 Errors

The compiler stops at the first phase that fails and says which phase that
was. The process exit code identifies it:

| Exit | Phase | Meaning |
|------|-------|---------|
| `0` | — | Success |
| `1` | — | Bad arguments, or the `.dsl` file could not be opened |
| `2` | Lexical / syntax | The program is not in the language |
| `3` | Semantic | The program parses but does not mean anything valid |
| `4` | Runtime | Execution failed — missing file, malformed CSV, UniProt unreachable |

**Lexical errors** name the offending character and line:

```
Lexical Error: unexpected character '@' at line 2
```

**Syntax errors** name the line:

```
Syntax Error at line 3: syntax error
```

**Semantic errors** are all reported together, not just the first:

| Message | Cause |
|---------|-------|
| `Unknown protein attribute 'X'` | `X` is not one of the six attributes |
| `Unknown protein attribute 'X' in DISPLAY` / `in SORT BY` | Same, in those clauses |
| `Unknown entity 'X' (the only queryable entity is 'proteins')` | `FIND genes` |
| `Cannot compare string attribute 'X' with numeric operator '>'` | `name > 500` |
| `Attribute 'length' expects a numeric value, got "abc"` | `length = "abc"` |
| `Attribute 'name' expects a string value, got 500` | `name = 500` |
| `FIND 'X' used before any LOAD; no dataset has been declared` | `FIND` with no data |
| `TOP must be a positive integer, got 0` | `TOP 0` |
| `LOAD UNIPROT TOP must be at most 500, got N` | Over the API cap |
| `LOAD UNIPROT requires a non-empty query string` | `LOAD UNIPROT ""` |

**Runtime errors** cover the world outside the program: `could not open
dataset file`, `malformed row at line N (expected 6 columns, got M)`,
`non-numeric Length at line N`, `UniProt returned no entries for this query`,
and a failed UniProt request.

### 3.9 Worked examples

Filter and project over the bundled CSV:

```
LOAD "dataset/proteins.csv"

FIND proteins
WHERE organism = "Human" AND length > 300
DISPLAY name length function
```

`OR`, ordering, and a limit:

```
LOAD "dataset/proteins.csv"

FIND proteins
WHERE organism = "Human" AND length > 500
OR organism = "Chicken"
SORT BY length DESC
DISPLAY proteinid name organism length
```

Motif search and a count, over the same loaded data:

```
FIND proteins SEARCH "MVLSPADK" DISPLAY name length
FIND proteins WHERE organism = "Human" COUNT
```

Live data:

```
LOAD UNIPROT "kinase AND organism_id:9606 AND reviewed:true" TOP 100

FIND proteins
WHERE length > 800
SORT BY length DESC
TOP 10
DISPLAY proteinid name length function
```

All four are in [`sample_queries/`](sample_queries).

---

## 4. Architecture

```
 DSL source program
        │
        ▼
   Lexer  (Flex)          Phase 1 — characters → tokens
        │                          lexer.l
        ▼
   Parser (Bison)         Phase 2 — tokens → AST, LALR(1)
        │                          parser.y, ast.h/.cpp
        ▼
 Semantic Analyzer        Phase 3 — entity, attributes, types,
        │                          TOP, LOAD-before-FIND
        │                          semantic.h/.cpp
        ▼
 Execution Engine         Phase 4 — load CSV/UniProt, filter,
        │                          sort, truncate, project
        │                          executor.h/.cpp
        ▼
     Output               table · COUNT · JSON
```

`pipeline.cpp` wires the four phases together **once**. Two front ends sit on
top of it:

| Front end | Lexer input | Output | Used by |
|-----------|-------------|--------|---------|
| [`main.cpp`](main.cpp) | a `FILE*` (file or stdin) | phase report or JSON | the CLI |
| [`wasm_api.cpp`](wasm_api.cpp) | a JavaScript string | JSON | the browser |

Everything after the lexer input is *the same code*, which is why the browser
and the command line give identical answers for identical programs.

---

## 5. Building and running

Requires `flex`, `bison`, and a C++17 compiler. `make wasm` additionally needs
Emscripten — and therefore Python, since Emscripten is written in Python.
Plain `make` does not.

```bash
make                  # -> bin/proteindsl
make run-valid        # runs sample_queries/valid_query.dsl
make run-invalid      # runs the deliberately broken sample
make wasm             # -> docs/, the browser build
make serve            # builds docs/ and serves it on :8080
make clean            # removes build/ and bin/
```

### Command-line interface

```
bin/proteindsl [options] <path-to-.dsl-file | ->
```

| Option | Effect |
|--------|--------|
| `--json` | Emit one JSON document (AST, errors, results) instead of the phase report |
| `--cache-dir <dir>` | Where fetched UniProt responses are kept (default `.cache/`) |

A path of `-` reads the program from standard input:

```bash
echo 'LOAD "dataset/proteins.csv"
FIND proteins WHERE organism = "Human" COUNT' | ./bin/proteindsl -
```

### Expected output

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

And the failing sample:

```
== Phase 3: Semantic Analysis ==
Semantic Error: Unknown protein attribute 'unknown_field'
Semantic Error: Cannot compare string attribute 'name' with numeric operator '>'
Semantic Error: Attribute 'name' expects a string value, got 500

Semantic analysis failed; stopping before execution.
```

### The `--json` document

```json
{
  "ok": true,
  "stage": "done",
  "statements": 2,
  "ast": "Statement 1: LOAD\n  file: ...",
  "errors": [],
  "sources": ["dataset/proteins.csv -> 5 record(s)"],
  "results": [{
    "entity": "proteins",
    "kind": "display",
    "matched": 3,
    "returned": 2,
    "columns": ["name", "length"],
    "rows": [["Albumin", "609"], ["Hemoglobin", "574"]]
  }]
}
```

`stage` is one of `parse`, `semantic`, `runtime`, `done`, so a caller can tell
a typo apart from UniProt being unreachable.

---

## 6. The web playground

The page ([`web/`](web)) is a textarea, the sample queries as one-click
examples, the results as tables, and the parsed AST beside them.
`Ctrl`+`Enter` runs.

There is no server. `make wasm` compiles the compiler to WebAssembly next to
the page, and `app.js` calls into it directly, so every phase runs in the tab
and nothing is transmitted. `docs/` is an ordinary static directory — which is
why GitHub Pages can host it and why any web server will do locally.

The browser build differs from the native one in exactly three places, all
inside `#ifdef __EMSCRIPTEN__`:

- **`LOAD UNIPROT`** calls the page's `fetch()` instead of shelling out to
  `curl`. Emscripten's ASYNCIFY suspends the WebAssembly stack across the
  `await`, so the executor keeps its ordinary synchronous shape. UniProt sends
  `Access-Control-Allow-Origin: *`, so no proxy is needed.
- **Caching** is in memory for the life of the tab, rather than in `.cache/`.
- **`dataset/`** is preloaded into Emscripten's virtual filesystem, so
  `LOAD "dataset/proteins.csv"` works unchanged.

---

## 7. Deployment

GitHub Pages serves static files, so a compiler normally could not run there.
Building it to WebAssembly removes the problem — the same approach
`onnxruntime-web` uses to run a neural network in a browser.

[`.github/workflows/pages.yml`](.github/workflows/pages.yml) runs `make wasm`
on every push to `main` and publishes the result, so **no build artefact is
ever committed** — `docs/` is gitignored. The workflow pins the Emscripten
version, caches the toolchain, and also compiles the native build as a check
that both front ends still build from the same sources.

To build the site by hand:

```bash
git clone https://github.com/emscripten-core/emsdk && cd emsdk
./emsdk install latest && ./emsdk activate latest && source ./emsdk_env.sh
cd - && make wasm
```

---

## 8. Design decisions and limits

**Why `curl` instead of libcurl.** The executor shells out to `curl`, which
ships with Windows 10+ and every mainstream Linux distribution. The compiler
therefore has no third-party dependency and builds anywhere flex, bison and a
C++17 compiler exist. The UniProt query is percent-encoded before it reaches
the command line, so a query containing shell metacharacters is transmitted as
data rather than executed — and the DSL's string literals cannot contain a
quote in the first place.

**Why disjunctive normal form.** Storing `WHERE` as an OR of AND-groups means
the parser can build the final representation directly as it reduces, and the
executor's matcher is two nested loops with no tree walk.

**Why the type tag comes from the token.** Recording whether the lexer saw a
`STRING` or a `NUMBER` — rather than trying to parse the text later — is what
makes `length = "300"` an error instead of a silent success.

**UniProt text is cleaned.** The leading `FUNCTION: ` and the `{ECO:...}`
evidence tags are stripped from the function column, since they are metadata
rather than content. Every other field is passed through verbatim.

Known limits:

- `WHERE` has no parentheses; precedence is fixed at `AND` over `OR`.
- Ordering comparisons are meaningful only on `length`, the one numeric
  attribute, which semantic analysis enforces.
- One dataset is in scope at a time — a later `LOAD` replaces the previous one
  rather than joining to it.
- Cached UniProt responses never expire; delete `.cache/` to force a refetch.
- String literals have no escape sequences, so they cannot contain `"`.

---

## 9. Repository layout

```
ProteinDSL/
│
├── lexer.l                Phase 1: Flex lexer
├── parser.y               Phase 2: Bison grammar
├── ast.h / ast.cpp        AST node types + pretty-printer
├── semantic.h / .cpp      Phase 3: semantic checks
├── executor.h / .cpp      Phase 4: data loading, filtering, sorting, output
├── pipeline.h / .cpp      The four phases wired together, shared by both builds
├── main.cpp               Front end 1: the native CLI
├── wasm_api.cpp           Front end 2: the WebAssembly entry point
├── Makefile               Native build, wasm build, run targets
│
├── dataset/
│   └── proteins.csv       Five sample protein records
│
├── sample_queries/
│   ├── valid_query.dsl      The basic end-to-end example
│   ├── advanced_query.dsl   OR / SORT BY / TOP / SEARCH / COUNT
│   ├── uniprot_query.dsl    Live UniProt search
│   └── invalid_query.dsl    Deliberate semantic errors
│
├── web/                   The playground: index.html, app.css, app.js
├── tools/
│   └── make_examples.py   Generates examples.json from sample_queries/
└── .github/workflows/
    └── pages.yml          Builds and deploys the site
```

`docs/` appears after `make wasm`. It is gitignored — CI rebuilds it.

---

## License

For academic / coursework use.
