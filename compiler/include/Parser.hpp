#ifndef PARSER_HPP_
#define PARSER_HPP_

#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "AST.hpp"
#include "Location.hpp"
#include "Scanner.hpp"

using std::initializer_list;
using std::runtime_error;
using std::string;
template<typename T>
using uptr = std::unique_ptr<T>;
template<typename T>
using sptr = std::shared_ptr<T>;
using std::vector;

class Parser {
  public:
  explicit Parser(Scanner &scanner_) : scanner(scanner_) {}
  sptr<Program> parse();

  private:
  Scanner &scanner;
  Token currentToken;
  Token lastToken;

  private:
  uptr<ASTNode> parseDeclarations();
  uptr<FuncDecl> parseFuncDecl();
  uptr<FuncDef> parseFuncDef();
  uptr<Header> parseHeader();
  vector<uptr<FuncParameterDecl>> parseParameters();
  uptr<FuncParameterDecl> parseFuncParameterDecl();
  uptr<FuncParameterType> parseFuncParameterType(bool is_ref);
  uptr<VarDef> parseVarDef();
  uptr<ClassDecl> parseClassDef();
  uptr<Type> parseType();
  DataType::DataType parseDataType();
  uptr<Block> parseBlock();
  uptr<Stmt> parseStmt();
  uptr<Stmt> parseAssignmentOrProcCall();
  uptr<Stmt> parseExprStmt();
  uptr<IfStmt> parseIfStmt();
  uptr<LoopStmt> parseLoopStmt();
  uptr<Lval> parseLVal();
  uptr<Expr> parseCall();
  uptr<Expr> parseExpr();
  uptr<Expr> parseLogicalOr();
  uptr<Expr> parseLogicalAnd();
  uptr<Expr> parseEquality();
  uptr<Expr> parseRelational();
  uptr<Expr> parseAdditive();
  uptr<Expr> parseMultiplicative();
  uptr<Expr> parseUnary();
  uptr<Expr> parsePrimary();
  std::vector<uptr<Expr>> parseArguments();
  uptr<Cond> parseCond();
  // uptr<Cond> parseLogicalOrCond();
  // uptr<Cond> parseLogicalAndCond();
  // uptr<Cond> parseUnaryCond();
  // uptr<Cond> parsePrimaryCond();


  // helper functions
  bool match(const initializer_list<TokenType> &types);
  bool check(TokenType type);
  Token advance();
  bool isAtEnd();
  Token peek() const;
  Token previous() const;
  Token consume(TokenType type, string message);
  Location currentLocation() const;
  runtime_error error(Token token, string message);
  void synchronize();
};

#endif// PARSER_HPP_
