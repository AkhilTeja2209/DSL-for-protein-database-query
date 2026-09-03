# Presenting ProteinDSL — a four-way division

The project splits cleanly along the four classical compiler phases, so each
person owns one phase end to end and can answer for it without depending on
anyone else's notes.

- [Shared ground](#shared-ground--everyone-must-know-this)
- [Person 1 — Language design & Lexical Analysis](#person-1--language-design--lexical-analysis-phase-1)
- [Person 2 — Syntax Analysis & the AST](#person-2--syntax-analysis--the-ast-phase-2)
- [Person 3 — Semantic Analysis & the Driver](#person-3--semantic-analysis--the-driver-phase-3)
- [Person 4 — Execution Engine & Deployment](#person-4--execution-engine--deployment-phase-4)
- [Running order and timing](#running-order-and-timing)
- [Demo script](#demo-script)
- [Questions anyone might get](#questions-anyone-might-get)

Detail for every file is in [WALKTHROUGH.md](WALKTHROUGH.md); the language
itself is in [README.md](README.md#3-language-documentation).

---

## Shared ground — everyone must know this

Whoever is asked, these four answers should be immediate.

**What is the project?** A domain-specific language for querying protein
records, implemented as a real compiler: lexer → parser → semantic analyser →
execution engine. It runs as a command-line tool and, compiled to
WebAssembly, entirely inside a browser.

**Why a DSL and not SQL?** The queries are written in the vocabulary of the
domain (`FIND proteins WHERE organism = "Human" AND length > 300`) with no
schema to learn. The real point is that it is a vehicle for building every
compiler phase against a problem with actual data behind it.

**The pipeline, in one breath.** `lexer.l` turns characters into tokens;
`parser.y` turns tokens into an AST; `semantic.cpp` checks the AST means
something; `executor.cpp` runs it against a CSV or live UniProt data;
`pipeline.cpp` sequences all four exactly once.

**Where does it run?** Both. `main.cpp` and `wasm_api.cpp` are two front ends
onto the same `pipeline.cpp` — the only difference is whether Flex reads from
a `FILE*` or a JavaScript string. That shared core is why the browser and the
command line cannot give different answers.

---

## Person 1 — Language design & Lexical Analysis (Phase 1)

**Owns:** [`lexer.l`](lexer.l) (72 lines), [`dataset/proteins.csv`](dataset),
[`sample_queries/`](sample_queries), and the opening of the talk.

### Segment outline

1. **The problem** — protein databases are queried through web forms (can't
   express compound conditions) or SQL (schema has nothing to do with
   biology). A small purpose-built language fits between them.
2. **The language, by example** — show `valid_query.dsl`, read it aloud. It
   should need no explanation; that is the argument for a DSL.
3. **The token set** — 14 keywords, 6 comparison operators, identifiers,
   string and number literals, `//` comments.
4. **How Flex decides** — longest match wins; ties break by rule order. That
   single rule is why `LOAD` is a keyword and `loaded_at` is an identifier,
   and why `>=` never lexes as `>` followed by `=`.
5. **Lexical error handling** — the catch-all rule reports the character and
   line number, and the run stops rather than silently skipping input.

### The thing worth showing

The string-literal rule, because it does real work in three lines:

```c
\"[^\"]*\"    { yylval.strval = strdup(yytext + 1);
                yylval.strval[strlen(yylval.strval) - 1] = '\0';
                return STRING; }
```

`yytext + 1` skips the opening quote; overwriting the last character removes
the closing one. The parser receives the string's contents, never its
punctuation.

### Expect to be asked

**"Why `%option noyywrap`?"** There is no second input file after the current
one ends, so we do not need `yywrap()` — and without the option we would have
to link `libfl` purely to supply it.

**"Why track `lex_errors` yourself?"** Flex's default on an unmatched
character is to echo it and carry on. We want the compilation to *fail*, so
the catch-all rule counts errors and `pipeline.cpp` checks the count.

**"Can a string contain a quote?"** No — the pattern is "quote, non-quotes,
quote", and there are no escape sequences. It is a documented limitation, and
it has a side benefit: a string literal can never carry a quote into the shell
command that fetches UniProt data.

**"Why are keywords case-sensitive but attributes not?"** Keywords are fixed
tokens matched literally by Flex. Attribute names are compared through
`toLowerField` at semantic-analysis time, so `Length` and `length` are the
same attribute. It is a deliberate split: the language is uppercase, the data
vocabulary is forgiving.

---

## Person 2 — Syntax Analysis & the AST (Phase 2)

**Owns:** [`parser.y`](parser.y) (~264 lines), [`ast.h`](ast.h) (108),
[`ast.cpp`](ast.cpp) (77).

### Segment outline

1. **The grammar** — put the EBNF from
   [README §3.3](README.md#33-grammar) on a slide and walk it.
2. **Why it is unambiguous** — five optional clauses in one production, yet
   LALR(1) with **zero conflicts**, because each clause is introduced by a
   distinct keyword and the order is fixed. `bison -Wall` proves it.
3. **Operator precedence without precedence declarations** — two levels of
   left recursion (`or_expr` of `and_expr`s) give `AND` tighter binding than
   `OR`.
4. **The AST** — `Statement`, `Condition`, and the DNF representation of
   `WHERE`.
5. **The printer** — `printProgram` writes to an `ostream&`, which is how the
   AST reaches both the terminal and the browser panel.

### The thing worth showing

Disjunctive normal form. `a AND b OR c` is stored as `{{a, b}, {c}}`:

```cpp
using AndGroup    = std::vector<Condition>;
using WhereClause = std::vector<AndGroup>;   // OR of AND-groups
```

Two payoffs to name: the parser builds the final representation directly as it
reduces — no tree to flatten afterwards — and the executor's matcher becomes
two nested loops instead of a recursive walk. Show `matchWhere` next to it if
Person 4 is happy to share the slide.

### Expect to be asked

**"Why can the `%union` only hold pointers?"** Bison stores semantic values in
a C `union`, which cannot contain a type with a constructor or destructor.
`std::vector<Condition>*` is fine; `std::vector<Condition>` would not compile.
That is also why the actions `new` and `delete` by hand.

**"What is `kNoTop`?"** A sentinel of `-1.0` for an absent `TOP` clause, since
the union cannot hold a `std::optional`. `TOP` is validated as positive later,
so `-1` can never be a real value.

**"Why record `isNumeric` in the AST?"** It stores *which token the lexer
actually produced*, rather than re-reading the text later. It is the single
field that makes `length = "300"` a type error instead of a silent success —
worth stating as the design decision it is.

**"Show me it really is conflict-free."** `bison -Wall -d -o build/parser.tab.cpp parser.y`
prints nothing but `%empty` style notes. Have that in the terminal history.

---

## Person 3 — Semantic Analysis & the Driver (Phase 3)

**Owns:** [`semantic.h`](semantic.h)/[`.cpp`](semantic.cpp) (160 lines),
[`pipeline.h`](pipeline.h)/[`.cpp`](pipeline.cpp) (254),
[`main.cpp`](main.cpp) (102).

### Segment outline

1. **What parsing cannot catch** — `FIND proteins WHERE colour = "red"` is
   perfectly grammatical and completely meaningless.
2. **The checks** — unknown entity, unknown attribute (in `WHERE`, `SORT BY`
   and `DISPLAY`), operator/type mismatch in both directions, non-positive
   `TOP`, `LOAD UNIPROT` bounds, and `FIND` before any `LOAD`.
3. **All errors, not the first** — run `invalid_query.dsl` and show three
   messages at once.
4. **How the phases connect** — `runPipeline` is the sequence, written once.
5. **Two front ends, one core** — `main.cpp` sets `yyin`; `wasm_api.cpp` calls
   `yy_scan_string`. Everything after that is identical code.

### The thing worth showing

The `report` parameter, because it is why there is no duplicated logic:

```cpp
PipelineOutput runPipeline(const ExecOptions& opts,
                           std::ostream* report,
                           const std::string& sourceLabel);
```

Pass `&std::cout` and it narrates each phase live — which is what the CLI
wants, including progress before a slow network fetch. Pass `nullptr` and it
runs silently and the caller serialises the result as JSON. One function, two
output modes, no reimplementation.

Also worth 20 seconds: `resetPipelineState()`. The CLI runs one program per
process and would never need it; the WebAssembly module runs many per page
load, and Bison's state is global. Without the reset, the second query in a
browser tab would see the first query's statements.

### Expect to be asked

**"Why check both type directions?"** Checking only that `length` gets a
number lets `name = 300` through, and the executor would then compare `"300"`
against protein names — succeeding, silently, and wrongly. An error that
produces plausible output is worse than one that stops the run.

**"Why is `FIND`-before-`LOAD` a semantic check and not a syntax one?"** It
depends on the *order of statements*, which a context-free grammar cannot
express. It is the one check carrying state across statements — a
`bool seenLoad` — and it is a good illustration of where the phase boundary
falls.

**"Why does `semantic.h` export helper functions?"** So the executor can
include it and use the same `proteinFields()`, `isKnownField()` and
`isNumericField()`. One definition of the attribute list, shared by the phase
that validates and the phase that executes — they cannot drift apart.

**"What do the exit codes mean?"** `0` success, `1` bad arguments or
unreadable file, `2` lexical/syntax, `3` semantic, `4` runtime. A script can
tell what failed without parsing any text.

---

## Person 4 — Execution Engine & Deployment (Phase 4)

**Owns:** [`executor.h`](executor.h)/[`.cpp`](executor.cpp) (620 lines),
[`wasm_api.cpp`](wasm_api.cpp) (53), [`Makefile`](Makefile) (99),
[`web/`](web), [`.github/workflows/pages.yml`](.github/workflows/pages.yml).

This is the largest share. If the group prefers a flatter split, hand `web/`
and the CI workflow to Person 1, who has the lightest code load.

### Segment outline

1. **Execution order** — filter (`SEARCH` ∧ `WHERE`), sort, truncate,
   project. Naming the order explains why `COUNT` reports the post-`TOP`
   figure and why the output says `10 of 42 matched`.
2. **Two data sources, one record type** — a CSV row and a UniProt TSV entry
   both normalise to `ProteinRecord`, which is why every clause works
   identically against either.
3. **Live UniProt** — building the REST URL, percent-encoding, caching.
4. **WebAssembly** — how a C++ compiler ends up running in a browser tab.
5. **Deployment** — CI builds and publishes; no artefact in git.

### The things worth showing

**`runQuery` operates on pointers:**

```cpp
std::vector<const ProteinRecord*> hits;
```

Sorting moves pointers, not records — and each record holds a full amino-acid
sequence. The sort is `std::stable_sort`, so equal keys keep dataset order and
results are reproducible.

**`urlEncode` is a security control, not a formatting helper.** It
percent-encodes everything outside RFC 3986's unreserved set, so a UniProt
query containing `&`, `|` or backticks reaches `std::system` as
`[A-Za-z0-9-_.~%]` only, and cannot become shell syntax. Combined with the
lexer's inability to produce a string containing `"`, the injection surface is
closed twice. This is a good place to say it was tested, not assumed.

**ASYNCIFY is the clever bit.** In the browser there is no `curl`, so
`LOAD UNIPROT` calls `js_fetch_text`, declared with `EM_ASYNC_JS`:

```cpp
EM_ASYNC_JS(char*, js_fetch_text, (const char* url), {
    const response = await fetch(UTF8ToString(url));
    if (!response.ok) return 0;
    return stringToNewUTF8(await response.text());
});
```

Emscripten's ASYNCIFY transform suspends the WebAssembly stack across that
`await` and resumes it when the promise settles — which is why `loadUniProt`
stays an ordinary synchronous C++ function on both platforms. Only three
things differ inside `#ifdef __EMSCRIPTEN__`: the fetch, the cache location,
and the fact that `dataset/` is preloaded into a virtual filesystem.

### Expect to be asked

**"Why `curl` instead of libcurl?"** Linking libcurl means finding and
shipping a dependency on three platforms. The `curl` executable is already on
Windows 10+ and every mainstream Linux distribution, so the compiler stays
dependency-free. The cost is one process spawn per query, irrelevant beside
the network round-trip.

**"Isn't `std::system` dangerous?"** Yes, if you interpolate raw user input —
which is exactly why `urlEncode` runs first. See above.

**"How can GitHub Pages run a compiler? It's static hosting."** It doesn't.
The compiler is built to WebAssembly and runs in the visitor's browser; Pages
only serves files. Same principle as `onnxruntime-web` running a neural
network client-side.

**"How big is it?"** About 344 KB of WebAssembly plus 75 KB of loader.

**"How does UniProt work from a browser without a proxy?"** UniProt sends
`Access-Control-Allow-Origin: *`, so cross-origin requests from any page are
allowed. Verified with the deployed origin in the request header.

**"Why is `docs/` not in the repository?"** It is build output. CI runs
`make wasm` on every push and publishes the result, so nothing compiled is
ever committed. The workflow also builds the native binary and runs a sample,
so a change that breaks one front end but not the other cannot deploy green.

---

## Running order and timing

Budget for a 20-minute slot plus questions.

| # | Who | Segment | Minutes |
|---|-----|---------|---------|
| 1 | Person 1 | Problem, the language, Phase 1 lexing | 5 |
| 2 | Person 2 | Phase 2 grammar and the AST | 5 |
| 3 | Person 3 | Phase 3 checks, and how the phases connect | 4 |
| 4 | Person 4 | Phase 4 execution, UniProt, WebAssembly | 5 |
| 5 | Person 1 + 4 | Live demo | 3 |

Person 1 opens and Person 4 closes on the live site, so the talk begins and
ends with something running.

---

## Demo script

Rehearse this exact sequence. **Run the UniProt query once beforehand** so the
response is cached and the demo does not wait on the network — and say that
you are doing so, since caching is a feature worth naming.

**1 — The pipeline, on the command line** (Person 1)

```bash
./bin/proteindsl sample_queries/valid_query.dsl
```

Point at each phase banner in turn: lexing/parsing, then the printed AST, then
the semantic verdict, then the result table. The whole architecture is visible
in one screen of output.

**2 — Errors are caught at the right phase** (Person 3)

```bash
./bin/proteindsl sample_queries/invalid_query.dsl ; echo "exit code: $?"
```

Three semantic errors at once, exit code 3, and execution never starts.

Then a syntax error, to show a different phase failing:

```bash
echo 'LOAD "dataset/proteins.csv"
FIND proteins WHERE' | ./bin/proteindsl - ; echo "exit code: $?"
```

**3 — Live data** (Person 4)

```bash
./bin/proteindsl sample_queries/uniprot_query.dsl
```

100 records fetched from UniProt, filtered to those over 800 residues, sorted
longest-first, top 10.

**4 — The same compiler in a browser** (Person 4)

Open <https://akhilteja2209.github.io/DSL-for-protein-database-query/>,
click **Uniprot Query**, press **Run**. Show that the rows match the terminal
output from step 3 — same source, two build targets, identical answers. Then
open the network tab if you want to prove there is no backend: the only
outbound request is to `rest.uniprot.org`.

**If the network fails during the demo:** run `advanced_query.dsl` instead,
which uses only the bundled CSV and exercises `OR`, `SORT`, `SEARCH` and
`COUNT`. Have it ready.

---

## Questions anyone might get

**"How much of this is generated?"** Flex generates the lexer's state machine
from `lexer.l`, and Bison generates the LALR parser tables from `parser.y`.
Both are ~70 and ~264 lines of specification respectively. Everything after
the parser — the AST, all semantic analysis, the whole execution engine, both
front ends — is hand-written C++17, about 1,300 lines.

**"What would you add next?"** Parentheses in `WHERE`, which means an
expression tree instead of DNF and a precedence-climbing sub-grammar. Then
joins across two loaded datasets, which the current one-dataset-at-a-time
model deliberately does not support.

**"What is the weakest part?"** Honestly: `WHERE` has no parentheses, so
precedence is fixed at `AND` over `OR`; `=` on text is exact rather than
partial, which surprises people using UniProt data where organisms read
`Homo sapiens (Human)`; and cached UniProt responses never expire. All three
are documented in the README rather than hidden.

**"How did you test it?"** Every clause was run individually against the
bundled CSV with hand-checkable expected output; all four error phases were
triggered deliberately and checked for the right exit code; the browser build
was verified to return byte-identical rows to the native binary for the same
programs, including a live UniProt query; and CI compiles both front ends and
runs a sample on every push.

**"Why is there Python in a C++ project?"** Only in the build. Emscripten is
itself written in Python, so `make wasm` needs it either way; one 38-line
script generates the playground's example list from `sample_queries/`. Plain
`make` and the compiled binary never touch Python, and none ships to the
browser.
