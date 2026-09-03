// ast.cpp
// Implementation of the small AST helpers declared in ast.h.
#include "ast.h"
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
                out << "  limit: " << s.uniprotLimit << "\n";
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

        if (s.hasTop) out << "  TOP " << s.topN << "\n";

        if (s.output == OutputKind::COUNT) {
            out << "  COUNT\n";
        } else if (s.output == OutputKind::DISPLAY) {
            out << "  DISPLAY";
            for (const auto& f : s.displayFields) out << " " << f;
            out << "\n";
        }
    }
}
