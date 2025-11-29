// csv_tools.h

#ifndef CSV_TOOLS
#define CSV_TOOLS

#include <istream>
#include <string>
#include <vector>
#include <cctype>
#include <sstream>

enum class CSVState {
    UnquotedField,
    QuotedField,
    QuotedQuote
};

inline std::vector<std::string> readCSVRow(const std::string &row) {
    CSVState state = CSVState::UnquotedField;
    std::vector<std::string> fields {""};
    std::size_t i = 0; // index of the current field
    for (char c : row) {
        switch (state) {
            case CSVState::UnquotedField:
                switch (c) {
                    case ',': // end of field
                              fields.push_back(""); i++;
                              break;
                    case '"': state = CSVState::QuotedField;
                              break;
                    default:  fields[i].push_back(c);
                              break; }
                break;
            case CSVState::QuotedField:
                switch (c) {
                    case '"': state = CSVState::QuotedQuote;
                              break;
                    default:  fields[i].push_back(c);
                              break; }
                break;
            case CSVState::QuotedQuote:
                switch (c) {
                    case ',': // , after closing quote
                              fields.push_back(""); i++;
                              state = CSVState::UnquotedField;
                              break;
                    case '"': // "" -> "
                              fields[i].push_back('"');
                              state = CSVState::QuotedField;
                              break;
                    default:  // end of quote
                              state = CSVState::UnquotedField;
                              break; }
                break;
        }
    }
    return fields;
}

/// Read CSV file, Excel dialect. Accept "quoted fields ""with quotes"""
inline std::vector<std::vector<std::string>> readCSV(std::istream &in) {
    std::vector<std::vector<std::string>> table;
    std::string row;
    while (!in.eof()) {
        std::getline(in, row);
        if (in.bad() || in.fail()) {
            break;
        }
        auto fields = readCSVRow(row);
        // Optionally skip completely empty last line:
        if (fields.size() == 1 && fields[0].empty()) {
            continue;
        }
        table.push_back(fields);
    }
    return table;
}

// ------------------------------------------------------------------
// Helpers: numeric detection & CSV escaping (still fully generic)
// ------------------------------------------------------------------

// Simple numeric-string test: allows spaces, optional sign, one dot.
inline bool is_number_string(const std::string& s) {
    std::size_t i = 0;
    std::size_t n = s.size();

    // skip leading spaces
    while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    if (i == n) return false;

    // optional sign
    if (s[i] == '+' || s[i] == '-') {
        ++i;
        if (i == n) return false;
    }

    bool has_digit = false;
    bool has_dot   = false;

    for (; i < n; ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (std::isdigit(c)) {
            has_digit = true;
        } else if (c == '.') {
            if (has_dot) return false;
            has_dot = true;
        } else if (std::isspace(c)) {
            // allow trailing spaces only
            for (std::size_t j = i + 1; j < n; ++j) {
                if (!std::isspace(static_cast<unsigned char>(s[j]))) {
                    return false;
                }
            }
            break;
        } else {
            return false;
        }
    }

    return has_digit;
}

// Escape a field for CSV: add quotes if needed and double internal quotes.
inline std::string csv_escape_field(const std::string& s) {
    bool needs_quotes = false;
    for (char c : s) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes) {
        return s;
    }

    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '"') {
            out.push_back('"'); // escape as ""
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

#endif // CSV_TOOLS

// eof
