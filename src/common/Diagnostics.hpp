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

    enum class State {
        Lexing,
        Parsing,
        SemanticAnalysis,
    };

    struct Entry {
        Severity severity;
        State state;
        Location location;
        std::string message;
    };

    void report(Severity severity, State state, const Location &location, const std::string &message);

    void printALl() const;
    void clear();

    const std::vector<Entry> &getEntries() const { return entries; }

private:
    std::vector<Entry> entries;
    std::string fileName;
    std::size_t errorCount = 0;
    std::size_t warningCount = 0;
};