# UniProt response fixtures

These files stand in for live UniProt responses so the tests run offline and
give the same answer every time. The executor caches each response under
`uniprot-<FNV-1a of the request URL>.tsv`, and the test runner passes a copy of
this directory as `--cache-dir`, so a `LOAD UNIPROT` whose URL matches a file
here is served from it instead of the network.

The entries are **synthetic** (`TEST01`, `Test kinase 1`, ...): they mimic the
shape of UniProt's TSV, including the `FUNCTION:` prefix, `{ECO:...}` evidence
tags, CRLF line endings, an empty function column and an incomplete row, but
they are not real UniProt records.

| File | Request |
|------|---------|
| `uniprot-ab63f20d02eff6e7.tsv` | `LOAD UNIPROT "kinase AND organism_id:9606 AND reviewed:true" TOP 100` (the sample query) |
| `uniprot-39131ea9d99f0c29.tsv` | `LOAD UNIPROT "no such protein"`: a header with no entries |

If the request URL format in `executor.cpp` changes, these names change too
and the UniProt tests fail with a network error, which is the signal to
rename them.
