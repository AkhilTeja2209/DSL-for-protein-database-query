// executor.h
// -----------------------------------------------------------------------
// Phase 4: Query Execution.
//
// Pipeline for this phase: read a dataset (a local CSV, or a live UniProt
// search) -> store ProteinRecords -> apply SEARCH/WHERE -> SORT / TOP ->
// COUNT or DISPLAY the selected columns.
// -----------------------------------------------------------------------
#pragma once

#include <iosfwd>
#include <string>
#include <vector>
#include "ast.h"

// One protein record, whether it came from a CSV row or a UniProt entry.
struct ProteinRecord {
    std::string proteinId;
    std::string name;
    std::string organism;
    int         length = 0;
    std::string function;
    std::string sequence;
};

// The outcome of running one FIND statement.
struct QueryResult {
    std::string                           entity;
    OutputKind                            kind = OutputKind::DEFAULT;
    std::vector<std::string>              columns;  // lower-cased attribute names
    std::vector<std::vector<std::string>> rows;     // parallel to `columns`
    size_t                                matched = 0;   // rows passing SEARCH/WHERE
    size_t                                returned = 0;  // rows left after TOP
};

// Constraints applied to a whole program. The web server passes a dataset root
// so that a user-submitted program cannot read arbitrary files off the host.
struct ExecOptions {
    std::string datasetRoot;          // empty => file LOAD is unrestricted
    bool        allowNetwork = true;  // false => LOAD UNIPROT is refused
    std::string cacheDir = ".cache";  // where fetched UniProt TSVs are kept
};

// Reads a protein CSV (header: ProteinID,Name,Organism,Length,Function,Sequence)
// into memory. Returns false and fills `error` if the file can't be read, is
// outside `opts.datasetRoot`, or has a malformed row.
bool loadDataset(const std::string& path,
                 const ExecOptions& opts,
                 std::vector<ProteinRecord>& records,
                 std::string& error);

// Runs a UniProt REST search and parses the returned TSV into `records`.
// `sourceLabel` is filled with a human-readable description of where the data
// came from (including whether the local cache was used).
bool loadUniProt(const std::string& query,
                 long limit,
                 const ExecOptions& opts,
                 std::vector<ProteinRecord>& records,
                 std::string& sourceLabel,
                 std::string& error);

// Executes one FIND statement against already-loaded records.
bool runQuery(const Statement& stmt,
              const std::vector<ProteinRecord>& records,
              QueryResult& result,
              std::string& error);

// Renders a QueryResult as an aligned text table (or a count).
void printResult(const QueryResult& result, std::ostream& out);

// Escapes a string for embedding in JSON output (see main.cpp --json).
std::string jsonEscape(const std::string& s);
