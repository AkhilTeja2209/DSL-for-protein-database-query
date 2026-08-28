// semantic.h
// -----------------------------------------------------------------------
// Phase 3: Semantic Analysis (see project plan, slide 11).
//
// Validates that fields referenced in WHERE / DISPLAY are real protein
// attributes and that comparison operators are used with the right type
// (e.g. `>` only makes sense on the numeric Length attribute).
//
// Status: implemented for the attribute/type checks shown on slide 11 and
// slide 14 ("Unknown protein attribute", "Cannot compare string attribute
// with numeric operator"). Cross-statement checks (e.g. FIND without a
// prior LOAD) are not yet implemented -- see README "Current Status".
// -----------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>
#include "ast.h"

// Runs semantic checks over every FIND statement in `program`.
// Returns true if no semantic errors were found; otherwise `errors` is
// filled with one human-readable message per problem (mirrors the
// "Error: ..." examples on slide 14).
bool validateProgram(const Program& program, std::vector<std::string>& errors);
