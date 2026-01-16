#pragma once
#include "Location.hpp"
#include <cstddef>
#include <string>
#include <vector>

class Diagnostics {
public:
    enum class Severity {
        Info,
        Warning,
        Error
    };

    enum class Phase {
        Lexing,
        Parsing,
        SemanticAnalysis,
    };

    struct Entry {
        Severity severity;
        Phase phase;
        Location loc;
        std::string message;
    };

    void report(Severity severity, Phase phase, const Location &location, const std::string &message);
    void report(Severity severity, int line, int col, const char *fmt, ...);
    void printAll() const;
    void clear();

    const std::vector<Entry> &getEntries() const;
    std::size_t errorCount() const;
    std::size_t warningCount() const;
    bool hasErrors() const;


private:
    std::vector<Entry> entries;
    std::string fileName;
    std::size_t error_count = 0;
    std::size_t warning_count = 0;
};

namespace cat {

    // Global diagnostics pointer for lexer/parser integration
    void setGlobalDiagnostics(Diagnostics *diags);
    Diagnostics *getGlobalDiagnostics();

}// namespace cat