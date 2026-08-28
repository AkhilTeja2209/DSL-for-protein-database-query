// ast.cpp
// Implementation of the small AST helpers declared in ast.h.
#include "ast.h"
#include <iostream>

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

void printProgram(const Program& program) {
    for (size_t i = 0; i < program.size(); ++i) {
        const Statement& s = program[i];
        std::cout << "Statement " << (i + 1) << ": ";

        if (s.type == StmtType::LOAD) {
            std::cout << "LOAD\n";
            std::cout << "  file: \"" << s.loadFile << "\"\n";
            continue;
        }

        // StmtType::FIND
        std::cout << "FIND\n";
        std::cout << "  entity: " << s.entity << "\n";

        if (!s.conditions.empty()) {
            std::cout << "  WHERE";
            for (size_t c = 0; c < s.conditions.size(); ++c) {
                const Condition& cond = s.conditions[c];
                if (c > 0) std::cout << "\n    AND";
                std::cout << " " << cond.field << " "
                          << compareOpToString(cond.op) << " "
                          << (cond.isNumeric ? cond.value : ("\"" + cond.value + "\""));
            }
            std::cout << "\n";
        }

        if (!s.displayFields.empty()) {
            std::cout << "  DISPLAY";
            for (const auto& f : s.displayFields) std::cout << " " << f;
            std::cout << "\n";
        }
    }
}
