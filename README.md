# ProteinDSL

A Domain-Specific Language (DSL) for Protein Database Query and Analysis
— a Compiler Design project applying lexical analysis, syntax analysis,
semantic analysis and query execution to a bioinformatics problem.

> **Status: initial, partial implementation.** This is a demonstration of
> the architecture and pipeline from the project plan, not the finished
> compiler. See [Current Status](#current-status) for exactly what works
> today and what's left.

## What it does

ProteinDSL lets you query a CSV of protein records with a small,
readable language instead of general-purpose SQL:

```
LOAD "dataset/proteins.csv"

FIND proteins
WHERE organism = "Human"
AND length > 300

DISPLAY name length function
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
 Semantic Analyzer         Phase 3 - validates fields, operators, types
        |
        v
  Execution Engine         Phase 4 - reads the CSV, filters/sorts/selects
        |
        v
      Output
```

## Tech Stack

| Concern                     | Choice                                   |
|------------------------------|-------------------------------------------|
| Lexical analysis              | Flex                                      |
| Syntax analysis / parsing     | GNU Bison (`bison -d`, C++ output)        |
| Semantic analysis & execution | C++17                                     |
| Dataset storage               | CSV (`dataset/proteins.csv`)              |
| Data structures                | C++ STL (`vector`, `string`, file I/O)    |
| Build                          | GNU Make + g++                            |
| Target environment             | Linux / WSL (or any POSIX shell with flex/bison/g++) |

This matches the "Technology Stack" slide of the project plan exactly.

## Core Language Features

`LOAD`, `FIND`, `WHERE`, `AND` / `OR`, `COUNT`, `DISPLAY`, `SORT BY`,
`TOP`, `SEARCH` — all tokenized by the lexer today (see [Current
Status](#current-status) for which ones the parser and executor accept
so far).

## Project Structure

```
ProteinDSL/
│
├── lexer.l              Phase 1: Flex lexer
├── parser.y              Phase 2: Bison grammar
├── ast.h / ast.cpp       Internal representation (AST) + pretty-printer
├── semantic.h / .cpp     Phase 3: semantic checks
├── executor.h / .cpp     Phase 4: CSV loading + (stubbed) query execution
├── main.cpp              Driver: wires the pipeline together
├── Makefile
│
├── dataset/
│   └── proteins.csv      Sample protein records
│
├── sample_queries/
│   ├── valid_query.dsl    Runs the full pipeline successfully
│   └── invalid_query.dsl  Demonstrates semantic error reporting
│
└── README.md
```

## Building & Running

Requires `flex`, `bison`, and a C++17 compiler (`g++`).

```bash
# Debian/Ubuntu, if you don't already have them:
sudo apt-get install -y flex bison g++ make

make
./bin/proteindsl sample_queries/valid_query.dsl
./bin/proteindsl sample_queries/invalid_query.dsl
```

`make run-valid` and `make run-invalid` do the same thing.

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
  [executor] Loaded 5 protein record(s).
  [executor] 5 record(s) loaded and available for entity 'proteins'.
  [executor] filter / sort / display execution: NOT YET IMPLEMENTED.
```

### Expected output (invalid query)

```
== Phase 3: Semantic Analysis ==
Semantic Error: Unknown protein attribute 'unknown_field'
Semantic Error: Cannot compare string attribute 'name' with numeric operator '>'

Semantic analysis failed; stopping before execution.
```

## Current Status

This repo is an **initial demonstration**, deliberately scoped down from
the full compiler in the project plan, to show the architecture works
end to end before building out every phase.

Implemented and tested:
- **Lexer** — every keyword, operator, string/number literal from the
  spec is tokenized, with lexical-error reporting (line numbers).
- **Parser** — the core grammar from the example program: `LOAD`,
  `FIND ... WHERE <cond> (AND <cond>)* ... DISPLAY <fields>`, building a
  real AST, with syntax-error reporting.
- **Semantic analysis** — unknown-attribute and type-mismatch checks
  (e.g. `length` must be compared numerically, `name` must not be).
- **CSV loading** — `dataset/proteins.csv` is parsed into `ProteinRecord`
  structs.

Not yet implemented (next milestones, roughly following the
Implementation Plan in the project proposal):
- `OR` in `WHERE` clauses (only `AND` is wired into the grammar so far).
- `COUNT`, `SORT BY`, `TOP`, `SEARCH` — tokenized by the lexer already,
  not yet accepted by the parser grammar.
- Actual query execution: applying `WHERE` as a filter, `SORT BY`,
  `COUNT`, and printing only the `DISPLAY`-selected columns. Right now
  the executor only loads the CSV and reports the record count.
- Runtime error handling beyond a missing/malformed dataset file.

## License

For academic/coursework use.
