// semantic.cpp
#include "semantic.h"
#include <algorithm>
#include <cctype>
#include <set>

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Protein attributes, per the dataset design (slide 7).
const std::set<std::string>& knownFields() {
    static const std::set<std::string> fields = {
        "proteinid", "name", "organism", "length", "function", "sequence"
    };
    return fields;
}

// Only "length" is numeric; everything else is a string attribute.
bool isNumericField(const std::string& fieldLower) {
    return fieldLower == "length";
}

bool isNumericOp(CompareOp op) {
    return op == CompareOp::GT || op == CompareOp::LT ||
           op == CompareOp::GE || op == CompareOp::LE;
}

} // namespace

bool validateProgram(const Program& program, std::vector<std::string>& errors) {
    for (const Statement& s : program) {
        if (s.type != StmtType::FIND) continue;

        for (const Condition& cond : s.conditions) {
            const std::string fieldLower = toLower(cond.field);

            if (!knownFields().count(fieldLower)) {
                errors.push_back("Semantic Error: Unknown protein attribute '" +
                                  cond.field + "'");
                continue; // can't type-check an attribute that doesn't exist
            }

            bool fieldIsNumeric = isNumericField(fieldLower);

            if (isNumericOp(cond.op) && !fieldIsNumeric) {
                errors.push_back("Semantic Error: Cannot compare string attribute '" +
                                  cond.field + "' with numeric operator '" +
                                  compareOpToString(cond.op) + "'");
            }
            if (fieldIsNumeric && !cond.isNumeric) {
                errors.push_back("Semantic Error: Attribute '" + cond.field +
                                  "' expects a numeric value, got \"" + cond.value + "\"");
            }
        }

        for (const std::string& field : s.displayFields) {
            if (!knownFields().count(toLower(field))) {
                errors.push_back("Semantic Error: Unknown protein attribute '" +
                                  field + "' in DISPLAY");
            }
        }
    }

    return errors.empty();
}
