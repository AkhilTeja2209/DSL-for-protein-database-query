// executor.cpp
#include "executor.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace {

std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

} // namespace

bool loadDataset(const std::string& path,
                  std::vector<ProteinRecord>& records,
                  std::string& error) {
    std::ifstream in(path);
    if (!in.is_open()) {
        error = "Runtime Error: could not open dataset file '" + path + "'";
        return false;
    }

    std::string line;
    bool isHeader = true;
    int lineNo = 0;

    while (std::getline(in, line)) {
        ++lineNo;
        if (line.empty()) continue;

        if (isHeader) {
            // Expected: ProteinID,Name,Organism,Length,Function,Sequence
            isHeader = false;
            continue;
        }

        std::vector<std::string> f = splitCsvLine(line);
        if (f.size() < 6) {
            error = "Runtime Error: malformed row at line " + std::to_string(lineNo) +
                    " in '" + path + "' (expected 6 columns, got " +
                    std::to_string(f.size()) + ")";
            return false;
        }

        ProteinRecord rec;
        rec.proteinId = f[0];
        rec.name = f[1];
        rec.organism = f[2];
        try {
            rec.length = std::stoi(f[3]);
        } catch (...) {
            error = "Runtime Error: non-numeric Length at line " + std::to_string(lineNo);
            return false;
        }
        rec.function = f[4];
        rec.sequence = f[5];
        records.push_back(rec);
    }

    return true;
}

void runQuery(const Statement& stmt, const std::vector<ProteinRecord>& records) {
    // TODO(next milestone): apply stmt.conditions as an actual filter,
    // honor DISPLAY column selection, and add COUNT / SORT BY / TOP once
    // the parser (parser.y) accepts them. For now this only demonstrates
    // that the executor is wired up to the AST and the loaded dataset.
    std::cout << "  [executor] " << records.size()
              << " record(s) loaded and available for entity '" << stmt.entity << "'.\n";
    std::cout << "  [executor] filter / sort / display execution: NOT YET IMPLEMENTED.\n";
}
