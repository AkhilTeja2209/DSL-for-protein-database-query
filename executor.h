// executor.h
// -----------------------------------------------------------------------
// Phase 4: Query Execution (see project plan, slide 12).
//
// Pipeline for this phase: Read CSV -> Store ProteinRecords -> Apply
// Filter -> Sort / Count / Select -> Display Results.
//
// Status: "Read CSV" and "Store ProteinRecords" are implemented below
// (ProteinRecord + loadDataset). "Apply Filter", "Sort / Count / Select"
// and "Display Results" are stubbed out (runQuery) pending the semantic
// pass being wired into the parser output -- see README "Current Status"
// and Implementation Plan steps 6-7 (slide 17).
// -----------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>
#include "ast.h"

// One row of dataset/proteins.csv (columns per slide 7).
struct ProteinRecord {
    std::string proteinId;
    std::string name;
    std::string organism;
    int         length = 0;
    std::string function;
    std::string sequence;
};

// Reads a protein CSV file (header: ProteinID,Name,Organism,Length,Function,Sequence)
// into memory. Returns false and fills `error` if the file can't be read.
bool loadDataset(const std::string& path,
                  std::vector<ProteinRecord>& records,
                  std::string& error);

// Executes a single FIND statement against already-loaded records.
// NOT YET IMPLEMENTED: filtering, sorting, counting and column selection
// are the next milestone. For now this reports how many records are
// available and which query it would have run, so the pipeline is
// visibly wired end-to-end even though this stage is still a stub.
void runQuery(const Statement& stmt, const std::vector<ProteinRecord>& records);
