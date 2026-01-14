#include "../include/Logger.hpp"
#include <string>

namespace Error {
    std::vector<ErrorInfo> exceptionList{};
    std::vector<std::string> spnitList{};
    bool hadError = false;


    void addError(unsigned int line, unsigned int column, std::string where, std::string message) noexcept {
        exceptionList.emplace_back(line, column, std::move(where), std::move(message));
        hadError = true;
    }

    void addError(const Token &token, std::string message) noexcept {
        if (token.type == TokenType::TOKEN_EOF) {
            exceptionList.emplace_back(token.line, token.column, "at end", std::move(message));
        } else {
            exceptionList.emplace_back(token.line, token.column, "at '" + token.lexeme + "'", std::move(message));
        }

        hadError = true;
    }

    void addWarn(const Token &token, std::string message) noexcept {
        if (token.type == TokenType::TOKEN_EOF) {
            exceptionList.emplace_back(token.line, token.column, "at end", std::move(message));
        } else {
            exceptionList.emplace_back(token.line, token.column, "at '" + token.lexeme + "'", std::move(message));
        }
    }

    void report() noexcept {
        for (const auto &exception: exceptionList) {
            std::string current_line = spnitList.at(exception.line - 1);
            std::string indicator(exception.column - 2, ' ');
            indicator += "\033[31m^";// red: \033[31m green: \033[32m
            if (current_line.size() > exception.column) {
                indicator += std::string(current_line.size() - exception.column - 1, '~');
            }
            indicator += "\033[0m";// reset color

            std::cerr
                << "[Line " + std::to_string(exception.line) +
                       ", Col " + std::to_string(exception.column) +
                       "] :( " + exception.message + " " + exception.where
                << ".\n"
                << current_line
                << '\n'
                << indicator
                << '\n';
        }
    }

    std::vector<std::string> split(const std::string &input) {
        std::vector<std::string> result;
        std::istringstream stream(input);
        std::string line;
        while (std::getline(stream, line)) {
            result.push_back(line);
        }
        return result;
    }
}// namespace Error