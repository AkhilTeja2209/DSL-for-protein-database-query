// semantic.h
// -----------------------------------------------------------------------
// Phase 3: Semantic Analysis.
//
// Validates that fields referenced in SEARCH / WHERE / SORT BY / DISPLAY are
// real protein attributes, that comparison operators are used with the right
// type (e.g. `>` only makes sense on the numeric `length` attribute), that
// TOP is a positive integer, and that a FIND is preceded by a LOAD.
// -----------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>
#include "ast.h"

// Canonical protein attribute names, lower-cased. Shared with the executor so
// the two phases can never disagree about which attributes exist.
const std::vector<std::string>& proteinFields();

// Lower-cases `s`. Field names are compared case-insensitively throughout.
std::string toLowerField(const std::string& s);

// True if `fieldLower` (already lower-cased) is a protein attribute.
bool isKnownField(const std::string& fieldLower);

// True if `fieldLower` holds a number. Only `length` does.
bool isNumericField(const std::string& fieldLower);

// Runs semantic checks over `program`. Returns true if no errors were found;
// otherwise `errors` holds one human-readable message per problem.
bool validateProgram(const Program& program, std::vector<std::string>& errors);
