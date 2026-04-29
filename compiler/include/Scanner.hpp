#ifndef SCANNER_HPP_
#define SCANNER_HPP_

#include "Token.hpp"
#include <llvm-20/llvm/ADT/StringMap.h>
#include <string>
#include <vector>
using std::string;
using std::vector;

class Scanner {
  private:
  string source;
  static const llvm::StringMap<TokenType> keywords;
  size_t start = 0;
  size_t current = 0;
  int line = 1;
  int column = 1;
  char advance();
  Token makeToken(TokenType type);
  Token makeToken(TokenType type, string value);
  bool match(char expected);

  Token String();
  Token Number();
  Token identifier();

  char peek() const;
  char peekNext() const;
  inline bool isAlpha(char c) const;
  inline bool isAlphaNumeric(char c) const;
  inline bool isDigit(char c) const;
  inline bool isAtEnd() const;

  public:
  explicit Scanner(string source);
  Token scanToken();
  //vector<Token> scanTokens();
};

#endif// SCANNER_HPP_
