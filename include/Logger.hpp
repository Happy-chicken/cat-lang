#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "Token.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <system_error>
#include <vector>
#define DIE Error::ErrorLogMessage()
namespace Error {
    class ErrorLogMessage : public std::basic_ostringstream<char> {
    public:
        ~ErrorLogMessage() {
            std::cerr << "Fatal error: " << str().c_str() << std::endl;
            exit(EXIT_FAILURE);
        }
    };

    // static std::string source;// record source code

    struct ErrorInfo {
        const unsigned int line;
        const unsigned int column;
        const std::string where = "";
        const std::string message;

        ErrorInfo(unsigned int line, unsigned int column, std::string where, std::string message)
            : line{line}, column{column}, where{std::move(where)}, message{std::move(message)} {
        }
    };

    void report() noexcept;

    void addError(unsigned int line, unsigned int column, std::string where, std::string message) noexcept;
    void addError(const Token &token, std::string message) noexcept;
    void addWarn(const Token &token, std::string message) noexcept;

    std::vector<std::string> split(const std::string &input);// helper fucntion split by new line

    extern bool hadError;
    extern std::vector<ErrorInfo> exceptionList;
    extern std::vector<std::string> spnitList;
}// namespace Error

#endif// LOGGER_HPP