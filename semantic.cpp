// semantic.cpp
#include "semantic.h"
#include <algorithm>
#include <cctype>

namespace {

bool isNumericOp(CompareOp op) {
    return op == CompareOp::GT || op == CompareOp::LT ||
           op == CompareOp::GE || op == CompareOp::LE;
}

// UniProt's REST API caps a single TSV page at 500 entries; asking for more
// would silently return fewer, so reject it up front instead.
const long kMaxUniProtLimit = 500;

} // namespace

const std::vector<std::string>& proteinFields() {
    static const std::vector<std::string> fields = {
        "proteinid", "name", "organism", "length", "function", "sequence"
    };
    return fields;
}

std::string toLowerField(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool isKnownField(const std::string& fieldLower) {
    const std::vector<std::string>& fields = proteinFields();
    return std::find(fields.begin(), fields.end(), fieldLower) != fields.end();
}

bool isNumericField(const std::string& fieldLower) {
    return fieldLower == "length";
}

bool validateProgram(const Program& program, std::vector<std::string>& errors) {
    bool seenLoad = false;

    for (const Statement& s : program) {
        if (s.type == StmtType::LOAD) {
            seenLoad = true;
            if (s.loadSource == LoadSource::UNIPROT) {
                if (s.uniprotLimit <= 0) {
                    errors.push_back("Semantic Error: LOAD UNIPROT TOP must be a "
                                     "positive integer, got " +
                                     std::to_string(s.uniprotLimit));
                } else if (s.uniprotLimit > kMaxUniProtLimit) {
                    errors.push_back("Semantic Error: LOAD UNIPROT TOP must be at most " +
                                     std::to_string(kMaxUniProtLimit) + ", got " +
                                     std::to_string(s.uniprotLimit));
                }
                if (s.uniprotQuery.empty()) {
                    errors.push_back("Semantic Error: LOAD UNIPROT requires a non-empty "
                                     "query string");
                }
            } else if (s.loadFile.empty()) {
                errors.push_back("Semantic Error: LOAD requires a non-empty file path");
            }
            continue;
        }

        // --- FIND ---
        if (!seenLoad) {
            errors.push_back("Semantic Error: FIND '" + s.entity +
                             "' used before any LOAD; no dataset has been declared");
        }

        if (toLowerField(s.entity) != "proteins") {
            errors.push_back("Semantic Error: Unknown entity '" + s.entity +
                             "' (the only queryable entity is 'proteins')");
        }

        for (const AndGroup& group : s.where) {
            for (const Condition& cond : group) {
                const std::string fieldLower = toLowerField(cond.field);

                if (!isKnownField(fieldLower)) {
                    errors.push_back("Semantic Error: Unknown protein attribute '" +
                                     cond.field + "'");
                    continue;  // can't type-check an attribute that doesn't exist
                }

                const bool fieldIsNumeric = isNumericField(fieldLower);

                if (isNumericOp(cond.op) && !fieldIsNumeric) {
                    errors.push_back("Semantic Error: Cannot compare string attribute '" +
                                     cond.field + "' with numeric operator '" +
                                     compareOpToString(cond.op) + "'");
                }
                if (fieldIsNumeric && !cond.isNumeric) {
                    errors.push_back("Semantic Error: Attribute '" + cond.field +
                                     "' expects a numeric value, got \"" + cond.value + "\"");
                }
                if (!fieldIsNumeric && cond.isNumeric) {
                    errors.push_back("Semantic Error: Attribute '" + cond.field +
                                     "' expects a string value, got " + cond.value);
                }
            }
        }

        if (s.hasSort) {
            const std::string sortLower = toLowerField(s.sortField);
            if (!isKnownField(sortLower)) {
                errors.push_back("Semantic Error: Unknown protein attribute '" +
                                 s.sortField + "' in SORT BY");
            }
        }

        if (s.hasTop && s.topN <= 0) {
            errors.push_back("Semantic Error: TOP must be a positive integer, got " +
                             std::to_string(s.topN));
        }

        for (const std::string& field : s.displayFields) {
            if (!isKnownField(toLowerField(field))) {
                errors.push_back("Semantic Error: Unknown protein attribute '" +
                                 field + "' in DISPLAY");
            }
        }
    }

    return errors.empty();
}
