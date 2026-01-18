#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "Diagnostics.hpp"
#include "Scanner.hpp"
#include "Token.hpp"
#include <stdexcept>
using std::string;

const unordered_map<string, TokenType> Scanner::keywords = {
    {"and", AND},
    {"or", OR},
    {"false", FALSE},
    {"true", TRUE},

    {"class", CLASS},
    {"super", SUPER},
    {"self", SELF},

    {"if", IF},
    {"elif", ELIF},
    {"else", ELSE},

    {"for", FOR},
    {"while", WHILE},

    {"decl", DECL},
    {"def", DEF},
    {"var", VAR},

    {"none", NONE},

    {"ref", REF},
    {"return", RETURN},

    {"break", BREAK},
    {"continue", CONTINUE},

    {"int", INT},
    {"double", DOUBLE},
    {"bool", BOOL},
    {"str", STR},
    {"char", CHAR},
    {"list", LIST},
    // TODO
    {"lambda", LAMBDA},
    {"try", TRY},
    {"throw", THROW}
};

Scanner::Scanner(string source) : source(std::move(source)) {}

// vector<Token> Scanner::scanTokens() {
//     while (!isAtEnd()) {
//         // We are at the beginning of the next lexeme.
//         start = current;
//         scanToken();
//     }

//     tokens.emplace_back(TOKEN_EOF, "", Object::make_obj(""), line, column);
//     return tokens;
// }

// helper function
inline bool Scanner::isAtEnd() const { return current >= source.size(); }

inline bool Scanner::isDigit(char c) const { return c >= '0' && c <= '9'; }

inline bool Scanner::isAlpha(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

inline bool Scanner::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

char Scanner::peek() const { return isAtEnd() ? '\0' : source.at(current); }

char Scanner::peekNext() const {
    return current + 1 >= source.size() ? '\0' : source.at(current + 1);
}

char Scanner::advance() {
    current++;
    column++;
    return source.at(current - 1);
}

Token Scanner::makeToken(TokenType type) {
    string text = source.substr(start, current - start);
    start = current;
    return Token(type, text, line, column);
}

bool Scanner::match(char expected) {
    if (isAtEnd())
        return false;
    if (source.at(current) != expected)
        return false;
    current++;
    column++;
    return true;
}

Token Scanner::scanToken() {
    while (true) {
        if (isAtEnd()) {
            return makeToken(TOKEN_EOF);
        }

        char c = advance();
        Token token{TOKEN_EOF, "", -1, -1};
        switch (c) {
            case '(':
                token = makeToken(LEFT_PAREN);
                break;
            case ')':
                token = makeToken(RIGHT_PAREN);
                break;
            case '{':
                token = makeToken(LEFT_BRACE);
                break;
            case '}':
                token = makeToken(RIGHT_BRACE);
                break;
            case '[':
                token = makeToken(LEFT_BRACKET);
                break;
            case ']':
                token = makeToken(RIGHT_BRACKET);
                break;
            case ',':
                token = makeToken(COMMA);
                break;
            case '.':
                token = makeToken(DOT);
                break;
            case '-': {
                if (match('>')) {
                    token = makeToken(ARROW);
                } else if (match('-')) {
                    token = makeToken(MINUS_MINUS);
                } else {
                    token = makeToken(MINUS);
                }
                break;
            }
            case '+':
                token = makeToken(match('+') ? PLUS_PLUS : PLUS);
                break;
            case ':':
                token = makeToken(COLON);
                break;
            case ';':
                token = makeToken(SEMICOLON);
                break;
            case '*':
                token = makeToken(STAR);
                break;
            case '^':
                token = makeToken(CARAT);
                break;
            case '%':
                token = makeToken(MODULO);
                break;
            case '\\':
                token = makeToken(BACKSLASH);
                break;
            case '!':
                token = makeToken(match('=') ? BANG_EQUAL : BANG);
                break;
            case '=':
                token = makeToken(match('=') ? EQUAL_EQUAL : EQUAL);
                break;
            case '<':
                token = makeToken(match('=') ? LESS_EQUAL : LESS);
                break;
            case '>':
                token = makeToken(match('=') ? GREATER_EQUAL : GREATER);
                break;
            case '/':
                if (match('/')) {
                    start += 2;// eat 2 char '//'
                    // A comment goes until the end of the line.
                    while (peek() != '\n' && !isAtEnd()) {
                        advance();
                        start++;
                    }
                }// TODO multi-comments/**/
                else if (match('*')) {
                    start += 2;// eat 2 char '/*'
                    while (!(peek() == '*' && peekNext() == '/') && !isAtEnd()) {
                        if (peek() == '\n') {
                            line++;
                            column = 1;
                        }
                        advance();
                        start++;
                    }
                    current += 2;// skip the close token, */
                } else {
                    token = makeToken(SLASH);
                }
                continue;
            case ' ':
            case '\0':
            case '\r':
            case '\t':
                start++;
                // Ignore whitespace.
                continue;
            case '\n': {
                line++;
                column = 1;
                start++;
                // break;
                continue;
            }
            case '"':
                token = String();
                break;
            default:
                if (isDigit(c)) {
                    token = Number();
                } else if (isAlpha(c)) {
                    token = identifier();
                } else {
                    auto Diagnostic = cat::getGlobalDiagnostics();
                    if (Diagnostic) {
                        Diagnostic->report(Diagnostics::Severity::Error, Diagnostics::Phase::Lexing, Location{line, column}, "Unexpected character '" + std::string(1, c) + "'.");
                    }
                }
                break;
        }

        return token;
    }
}

Token Scanner::String() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') {
            line++;
            column = 1;
        }
        advance();
    }

    // Unterminated string.
    if (isAtEnd()) {
        auto diag = cat::getGlobalDiagnostics();
        if (diag) {
            diag->report(Diagnostics::Severity::Error, Diagnostics::Phase::Lexing, Location{line, column}, "Unterminated string.");
            throw std::runtime_error("Lexing failed");
        }
    }

    // The closing ".
    advance();

    // Trim the surrounding quotes.
    string value = source.substr(start + 1, current - start - 2);
    if (value.length() == 1) {
        return makeToken(CHAR);
    }
    return makeToken(STRING);
}

Token Scanner::Number() {
    while (isDigit(peek()))
        advance();
    bool isInteger = true;
    // Look for a fractional part.
    if (peek() == '.' && isDigit(peekNext())) {
        // Consume the "."
        isInteger = false;
        advance();
        while (isDigit(peek()))
            advance();
    }

    // std::string numStr = source.substr(start, current - start);
    if (isInteger) {
        // int istr = std::stoi(numStr);
        return makeToken(INTEGEL);
    } else {
        // float fstr = std::stof(numStr);
        return makeToken(NUMBER);
    }
}

Token Scanner::identifier() {
    while (isAlphaNumeric(peek()))
        advance();
    const string text = source.substr(start, current - start);

    return makeToken(keywords.find(text) != keywords.end() ? keywords.at(text) : IDENTIFIER);
}
