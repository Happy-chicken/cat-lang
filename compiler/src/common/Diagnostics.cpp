#include "Diagnostics.hpp"
#include "Location.hpp"

#include <cstdarg>
#include <iostream>

namespace {
    const char *toString(Diagnostics::Severity s) {
        switch (s) {
            case Diagnostics::Severity::Info:
                return "info";
            case Diagnostics::Severity::Warning:
                return "warning";
            case Diagnostics::Severity::Error:
                return "error";
        }
        return "?";
    }

    const char *toString(Diagnostics::Phase p) {
        switch (p) {
            case Diagnostics::Phase::Lexing:
                return "lexing";
            case Diagnostics::Phase::Parsing:
                return "parsing";
            case Diagnostics::Phase::SemanticAnalysis:
                return "semantic";
        }
        return "?";
    }

    // Global diagnostics for lexer/parser
    Diagnostics *g_diagnostics = nullptr;
}// namespace

namespace cat {

    void setGlobalDiagnostics(Diagnostics *diags) {
        g_diagnostics = diags;
    }

    Diagnostics *getGlobalDiagnostics() {
        return g_diagnostics;
    }

}// namespace cat

void Diagnostics::report(Severity severity, Phase phase, const Location &loc, const std::string &message) {
    entries.push_back(Entry{severity, phase, loc, message});
    if (severity == Severity::Error) {
        ++error_count;
    } else if (severity == Severity::Warning) {
        ++warning_count;
    }
}

void Diagnostics::report(Severity severity, int line, int col, const char *fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    Location loc{line, col};
    report(severity, Phase::Lexing, loc, std::string(buffer));
}

bool Diagnostics::hasErrors() const {
    return error_count > 0;
}

std::size_t Diagnostics::errorCount() const {
    return error_count;
}

std::size_t Diagnostics::warningCount() const {
    return warning_count;
}

const std::vector<Diagnostics::Entry> &Diagnostics::getEntries() const {
    return entries;
}

void Diagnostics::printAll() const {
    for (const auto &entry: entries) {

        std::cerr << entry.loc.line << ':' << entry.loc.column << ": "
                  << toString(entry.phase) << " "
                  << toString(entry.severity) << ": "
                  << entry.message << '\n';
    }
}

void Diagnostics::clear() {
    entries.clear();
    error_count = 0;
    warning_count = 0;
}
