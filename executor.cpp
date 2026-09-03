// executor.cpp
#include "executor.h"
#include "semantic.h"   // proteinFields(), toLowerField(), isNumericField()

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <istream>
#include <map>
#include <ostream>
#include <sstream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// The browser build has no curl and no sockets, so LOAD UNIPROT is served by
// the page's own fetch(). ASYNCIFY (see the `wasm` target in the Makefile)
// suspends the wasm stack across the await, which is what lets the executor
// keep its ordinary synchronous shape.
//
// Returns a malloc'd UTF-8 string the caller frees, or 0 on any failure.
EM_ASYNC_JS(char*, js_fetch_text, (const char* url), {
    try {
        const response = await fetch(UTF8ToString(url));
        if (!response.ok) return 0;
        return stringToNewUTF8(await response.text());
    } catch (e) {
        return 0;
    }
});
#endif

namespace fs = std::filesystem;

namespace {

// ---------------------------------------------------------------- text utils

std::string toLower(const std::string& s) { return toLowerField(s); }

bool containsCI(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Splits one delimited line. `quoted` enables RFC-4180 style "..." fields with
// doubled quotes, which CSV needs and UniProt's TSV does not use.
std::vector<std::string> splitLine(const std::string& line, char delim, bool quoted) {
    std::vector<std::string> fields;
    std::string cur;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quoted && c == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                cur.push_back('"');  // escaped quote
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == delim && !inQuotes) {
            fields.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    fields.push_back(cur);

    for (std::string& f : fields) f = trim(f);
    return fields;
}

// ------------------------------------------------------------- field access

std::string fieldValue(const ProteinRecord& r, const std::string& fieldLower) {
    if (fieldLower == "proteinid") return r.proteinId;
    if (fieldLower == "name")      return r.name;
    if (fieldLower == "organism")  return r.organism;
    if (fieldLower == "length")    return std::to_string(r.length);
    if (fieldLower == "function")  return r.function;
    if (fieldLower == "sequence")  return r.sequence;
    return "";
}

// ----------------------------------------------------------------- filtering

bool matchCondition(const ProteinRecord& rec, const Condition& cond) {
    const std::string fieldLower = toLower(cond.field);

    if (isNumericField(fieldLower)) {
        const double lhs = static_cast<double>(rec.length);
        const double rhs = std::strtod(cond.value.c_str(), nullptr);
        switch (cond.op) {
            case CompareOp::EQ: return lhs == rhs;
            case CompareOp::NE: return lhs != rhs;
            case CompareOp::GT: return lhs >  rhs;
            case CompareOp::LT: return lhs <  rhs;
            case CompareOp::GE: return lhs >= rhs;
            case CompareOp::LE: return lhs <= rhs;
        }
        return false;
    }

    // String attributes: `=` and `!=` are case-insensitive exact matches.
    // The ordering operators are rejected during semantic analysis, so they are
    // only reachable if that phase is skipped; fall back to a case-insensitive
    // lexicographic comparison rather than silently returning false.
    const std::string lhs = toLower(fieldValue(rec, fieldLower));
    const std::string rhs = toLower(cond.value);
    switch (cond.op) {
        case CompareOp::EQ: return lhs == rhs;
        case CompareOp::NE: return lhs != rhs;
        case CompareOp::GT: return lhs >  rhs;
        case CompareOp::LT: return lhs <  rhs;
        case CompareOp::GE: return lhs >= rhs;
        case CompareOp::LE: return lhs <= rhs;
    }
    return false;
}

// WHERE is stored in DNF: the record matches if every condition in at least one
// AND-group matches. An empty WHERE matches everything.
bool matchWhere(const ProteinRecord& rec, const WhereClause& where) {
    if (where.empty()) return true;
    for (const AndGroup& group : where) {
        bool all = true;
        for (const Condition& cond : group) {
            if (!matchCondition(rec, cond)) { all = false; break; }
        }
        if (all) return true;
    }
    return false;
}

// SEARCH is a free-text, case-insensitive substring scan over every attribute,
// which also makes it a convenient (if naive) sequence-motif search.
bool matchSearch(const ProteinRecord& rec, const std::string& term) {
    for (const std::string& f : proteinFields()) {
        if (containsCI(fieldValue(rec, f), term)) return true;
    }
    return false;
}

// -------------------------------------------------------------- UniProt fetch

std::string urlEncode(const std::string& s) {
    // Deliberately conservative: everything outside the RFC 3986 unreserved set
    // is percent-encoded. The encoded query is interpolated into a shell command
    // below, so this is also what stops a hostile query string (quotes, &, |,
    // backticks) from becoming shell syntax.
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

#ifndef __EMSCRIPTEN__
// FNV-1a, used only to give each distinct query a stable cache filename. The
// browser build caches by URL in memory instead, so it needs no filename.
std::string hashHex(const std::string& s) {
    unsigned long long h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    std::ostringstream os;
    os << std::hex << std::setw(16) << std::setfill('0') << h;
    return os.str();
}
#endif

// UniProt annotates function text with a leading "FUNCTION: " and evidence tags
// like {ECO:0000269|PubMed:...}. Both are metadata rather than content, so they
// are stripped to keep the column readable. Everything else is left verbatim.
std::string cleanFunction(const std::string& raw) {
    std::string s = raw;
    const std::string prefix = "FUNCTION: ";
    if (s.rfind(prefix, 0) == 0) s = s.substr(prefix.size());

    std::string out;
    int depth = 0;
    for (char c : s) {
        if (c == '{') { ++depth; continue; }
        if (c == '}') { if (depth > 0) --depth; continue; }
        if (depth == 0) out.push_back(c);
    }

    // Collapse the whitespace left behind by the removed tags.
    std::string collapsed;
    bool prevSpace = false;
    for (char c : out) {
        const bool isSpace = (c == ' ' || c == '\t');
        if (isSpace && prevSpace) continue;
        collapsed.push_back(isSpace ? ' ' : c);
        prevSpace = isSpace;
    }
    return trim(collapsed);
}

// Parses UniProt's TSV from any stream: a cached file on the native build, an
// in-memory string in the browser build.
bool parseUniProtStream(std::istream& in,
                        std::vector<ProteinRecord>& records,
                        std::string& error) {
    std::string line;
    bool isHeader = true;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (isHeader) { isHeader = false; continue; }

        // Columns requested, in order:
        // accession, protein_name, organism_name, length, cc_function, sequence
        std::vector<std::string> f = splitLine(line, '\t', false);
        if (f.size() < 6) continue;  // skip entries UniProt returned incomplete

        ProteinRecord rec;
        rec.proteinId = f[0];
        rec.name      = f[1];
        rec.organism  = f[2];
        rec.length    = std::atoi(f[3].c_str());
        rec.function  = cleanFunction(f[4]);
        rec.sequence  = f[5];
        records.push_back(rec);
    }

    if (records.empty()) {
        error = "Runtime Error: UniProt returned no entries for this query";
        return false;
    }
    return true;
}

std::string buildUniProtUrl(const std::string& query, long limit) {
    return "https://rest.uniprot.org/uniprotkb/search"
           "?query=" + urlEncode(query) +
           "&fields=accession,protein_name,organism_name,length,cc_function,sequence"
           "&format=tsv"
           "&size=" + std::to_string(limit);
}

} // namespace

// ----------------------------------------------------------------- public API

std::string jsonEscape(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

bool loadDataset(const std::string& path,
                 const ExecOptions& opts,
                 std::vector<ProteinRecord>& records,
                 std::string& error) {
    std::string resolved = path;

    // With a dataset root configured, a file LOAD may only reach inside it.
    // That is what makes it safe to run a stranger's program from the web UI.
    if (!opts.datasetRoot.empty()) {
        std::error_code ec;
        const fs::path root = fs::weakly_canonical(fs::absolute(opts.datasetRoot), ec);
        const fs::path want = fs::weakly_canonical(fs::absolute(path), ec);
        if (ec) {
            error = "Runtime Error: could not resolve dataset path '" + path + "'";
            return false;
        }
        // An empty relative path means the two are unrelated; a leading ".."
        // means the target climbs out of the root. Compared component-wise so
        // this works with Windows' wide-character paths too.
        const fs::path rel = want.lexically_relative(root);
        if (rel.empty() || *rel.begin() == "..") {
            error = "Runtime Error: LOAD \"" + path + "\" is outside the permitted "
                    "dataset directory '" + opts.datasetRoot + "'";
            return false;
        }
        resolved = want.string();
    }

    std::ifstream in(resolved);
    if (!in.is_open()) {
        error = "Runtime Error: could not open dataset file '" + path + "'";
        return false;
    }

    std::string line;
    bool isHeader = true;
    int lineNo = 0;

    while (std::getline(in, line)) {
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        if (isHeader) {
            // Expected: ProteinID,Name,Organism,Length,Function,Sequence
            isHeader = false;
            continue;
        }

        std::vector<std::string> f = splitLine(line, ',', true);
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
            error = "Runtime Error: non-numeric Length at line " +
                    std::to_string(lineNo) + " in '" + path + "'";
            return false;
        }
        rec.function = f[4];
        rec.sequence = f[5];
        records.push_back(rec);
    }

    return true;
}

bool loadUniProt(const std::string& query,
                 long limit,
                 const ExecOptions& opts,
                 std::vector<ProteinRecord>& records,
                 std::string& sourceLabel,
                 std::string& error) {
    if (!opts.allowNetwork) {
        error = "Runtime Error: LOAD UNIPROT is disabled in this environment";
        return false;
    }

    const std::string url = buildUniProtUrl(query, limit);

#ifdef __EMSCRIPTEN__
    // Browser build: there is no curl and no disk, so the fetch is handed to
    // the page's own fetch() and the response is cached in memory for the life
    // of the tab. UniProt sends Access-Control-Allow-Origin: *, so this works
    // from any origin without a proxy.
    static std::map<std::string, std::string> memCache;

    auto it = memCache.find(url);
    if (it != memCache.end()) {
        sourceLabel = "UniProt \"" + query + "\" (cached)";
        std::istringstream in(it->second);
        return parseUniProtStream(in, records, error);
    }

    char* raw = js_fetch_text(url.c_str());
    if (!raw) {
        error = "Runtime Error: the UniProt request failed. Check your network "
                "connection and the query syntax.";
        return false;
    }
    const std::string body(raw);
    std::free(raw);

    std::istringstream in(body);
    if (!parseUniProtStream(in, records, error)) return false;

    memCache[url] = body;  // only successful responses are remembered
    sourceLabel = "UniProt \"" + query + "\" (fetched, limit " +
                  std::to_string(limit) + ")";
    return true;
#else
    std::error_code ec;
    fs::create_directories(opts.cacheDir, ec);
    const std::string cachePath = opts.cacheDir + "/uniprot-" + hashHex(url) + ".tsv";

    // A cached response that turned out to hold no entries is discarded rather
    // than kept, so a query is never permanently stuck on an empty result.
    auto parseOrDropCache = [&](const std::string& label) {
        sourceLabel = label;
        std::ifstream in(cachePath);
        if (in.is_open() && parseUniProtStream(in, records, error)) return true;
        if (!in.is_open()) {
            error = "Runtime Error: could not read the UniProt response cached at '" +
                    cachePath + "'";
        }
        in.close();
        std::error_code rm;
        fs::remove(cachePath, rm);
        return false;
    };

    if (fs::exists(cachePath)) {
        return parseOrDropCache("UniProt \"" + query + "\" (cached)");
    }

    // curl ships with Windows 10+ and every mainstream Linux distro, which keeps
    // the compiler itself dependency-free (no libcurl to find and link against).
    // Only the percent-encoded URL varies with user input -- see urlEncode.
    const std::string tmpPath = cachePath + ".part";
    const std::string cmd =
        "curl -s -S -f -L --max-time 60 -A \"ProteinDSL/1.0\" -o \"" +
        tmpPath + "\" \"" + url + "\"";

    const int rc = std::system(cmd.c_str());
    if (rc != 0) {
        fs::remove(tmpPath, ec);
        error = "Runtime Error: UniProt request failed (curl exit code " +
                std::to_string(rc) + "). Check network access and the query syntax.";
        return false;
    }

    fs::rename(tmpPath, cachePath, ec);
    if (ec) {
        error = "Runtime Error: could not store the UniProt response at '" +
                cachePath + "'";
        return false;
    }

    return parseOrDropCache("UniProt \"" + query + "\" (fetched, limit " +
                            std::to_string(limit) + ")");
#endif
}

bool runQuery(const Statement& stmt,
              const std::vector<ProteinRecord>& records,
              QueryResult& result,
              std::string& error) {
    (void)error;  // no runtime failure mode yet; kept for a stable signature

    result.entity = stmt.entity;
    result.kind = stmt.output;

    // --- filter: SEARCH and WHERE both have to pass ---
    std::vector<const ProteinRecord*> hits;
    for (const ProteinRecord& rec : records) {
        if (stmt.hasSearch && !matchSearch(rec, stmt.searchTerm)) continue;
        if (!matchWhere(rec, stmt.where)) continue;
        hits.push_back(&rec);
    }
    result.matched = hits.size();

    // --- sort ---
    if (stmt.hasSort) {
        const std::string key = toLower(stmt.sortField);
        const bool numeric = isNumericField(key);
        const bool desc = (stmt.sortDir == SortDir::DESC);
        std::stable_sort(hits.begin(), hits.end(),
            [&](const ProteinRecord* a, const ProteinRecord* b) {
                if (numeric) {
                    return desc ? (a->length > b->length) : (a->length < b->length);
                }
                const std::string va = toLower(fieldValue(*a, key));
                const std::string vb = toLower(fieldValue(*b, key));
                return desc ? (va > vb) : (va < vb);
            });
    }

    // --- TOP ---
    if (stmt.hasTop && static_cast<size_t>(stmt.topN) < hits.size()) {
        hits.resize(static_cast<size_t>(stmt.topN));
    }
    result.returned = hits.size();

    // --- column selection ---
    if (stmt.output == OutputKind::DISPLAY) {
        for (const std::string& f : stmt.displayFields) {
            result.columns.push_back(toLower(f));
        }
    } else {
        result.columns = proteinFields();
    }

    // COUNT reports a number, so it does not materialise rows.
    if (stmt.output == OutputKind::COUNT) return true;

    for (const ProteinRecord* rec : hits) {
        std::vector<std::string> row;
        row.reserve(result.columns.size());
        for (const std::string& col : result.columns) {
            row.push_back(fieldValue(*rec, col));
        }
        result.rows.push_back(std::move(row));
    }
    return true;
}

void printResult(const QueryResult& result, std::ostream& out) {
    if (result.kind == OutputKind::COUNT) {
        out << "  COUNT = " << result.returned << "\n";
        return;
    }

    if (result.rows.empty()) {
        out << "  (no records matched)\n";
        return;
    }

    // Long sequences would blow the table apart, so they are elided in the text
    // renderer only; --json always emits the full value.
    const size_t kMaxCell = 40;
    auto cell = [&](const std::string& v) {
        if (v.size() <= kMaxCell) return v;
        return v.substr(0, kMaxCell - 3) + "...";
    };

    std::vector<size_t> width(result.columns.size());
    for (size_t c = 0; c < result.columns.size(); ++c) {
        width[c] = result.columns[c].size();
    }
    for (const auto& row : result.rows) {
        for (size_t c = 0; c < row.size(); ++c) {
            width[c] = std::max(width[c], cell(row[c]).size());
        }
    }

    auto rule = [&]() {
        out << "  +";
        for (size_t c = 0; c < width.size(); ++c) {
            out << std::string(width[c] + 2, '-') << "+";
        }
        out << "\n";
    };

    rule();
    out << "  |";
    for (size_t c = 0; c < result.columns.size(); ++c) {
        out << " " << std::left << std::setw(static_cast<int>(width[c]))
            << result.columns[c] << " |";
    }
    out << "\n";
    rule();

    for (const auto& row : result.rows) {
        out << "  |";
        for (size_t c = 0; c < row.size(); ++c) {
            out << " " << std::left << std::setw(static_cast<int>(width[c]))
                << cell(row[c]) << " |";
        }
        out << "\n";
    }
    rule();

    out << "  " << result.returned << " row(s)";
    if (result.returned != result.matched) out << " of " << result.matched << " matched";
    out << "\n";
}
