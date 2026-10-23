// ast.cpp
// Implementation of the small AST helpers declared in ast.h.
#include "ast.h"
#include <cmath>
#include <cstdio>
#include <ostream>

const char* compareOpToString(CompareOp op) {
    switch (op) {
        case CompareOp::EQ: return "=";
        case CompareOp::GT: return ">";
        case CompareOp::LT: return "<";
        case CompareOp::GE: return ">=";
        case CompareOp::LE: return "<=";
        case CompareOp::NE: return "!=";
    }
    return "?";
}

bool isValidCount(double v) {
    return v >= 1 && v <= kMaxCount && v == std::floor(v);
}

std::string formatNumber(double v) {
    char buf[64];
    if (v == std::floor(v) && std::fabs(v) < 1e18) {
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
    } else if (v == std::floor(v)) {
        std::snprintf(buf, sizeof(buf), "%.0f", v);  // too big for long long
    } else {
        std::snprintf(buf, sizeof(buf), "%g", v);
    }
    return buf;
}

namespace {

void printWhere(const WhereClause& where, std::ostream& out) {
    // Mirrors the DNF stored in the AST: groups joined by OR, conditions
    // within a group joined by AND.
    for (size_t g = 0; g < where.size(); ++g) {
        out << (g == 0 ? "  WHERE" : "\n     OR");
        for (size_t c = 0; c < where[g].size(); ++c) {
            const Condition& cond = where[g][c];
            if (c > 0) out << "\n    AND";
            out << " " << cond.field << " " << compareOpToString(cond.op) << " "
                << (cond.isNumeric ? cond.value : ("\"" + cond.value + "\""));
        }
    }
    out << "\n";
}

} // namespace

void printProgram(const Program& program, std::ostream& out) {
    for (size_t i = 0; i < program.size(); ++i) {
        const Statement& s = program[i];
        out << "Statement " << (i + 1) << ": ";

        if (s.type == StmtType::LOAD) {
            if (s.loadSource == LoadSource::UNIPROT) {
                out << "LOAD UNIPROT\n";
                out << "  query: \"" << s.uniprotQuery << "\"\n";
                out << "  limit: " << formatNumber(s.uniprotLimitLiteral) << "\n";
            } else {
                out << "LOAD\n";
                out << "  file: \"" << s.loadFile << "\"\n";
            }
            continue;
        }

        // StmtType::FIND
        out << "FIND\n";
        out << "  entity: " << s.entity << "\n";

        if (s.hasSearch) out << "  SEARCH \"" << s.searchTerm << "\"\n";

        if (!s.where.empty()) printWhere(s.where, out);

        if (s.hasSort) {
            out << "  SORT BY " << s.sortField << " "
                << (s.sortDir == SortDir::DESC ? "DESC" : "ASC") << "\n";
        }

        if (s.hasTop) out << "  TOP " << formatNumber(s.topLiteral) << "\n";

        if (s.output == OutputKind::COUNT) {
            out << "  COUNT\n";
        } else if (s.output == OutputKind::DISPLAY) {
            out << "  DISPLAY";
            for (const auto& f : s.displayFields) out << " " << f;
            out << "\n";
        }
    }
}
