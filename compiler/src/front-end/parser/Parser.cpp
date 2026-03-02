#include <climits>
#include <initializer_list>
#include <math.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "AST.hpp"
#include "Diagnostics.hpp"
#include "Location.hpp"
#include "Operator.hpp"
#include "Parser.hpp"
#include "Symbol.hpp"
#include "Token.hpp"
#include "Types.hpp"


using std::initializer_list;
using std::make_shared;
using std::runtime_error;


sptr<Program> Parser::parse() {
  advance();
  Location loc = currentLocation();
  vec<uptr<ASTNode>> defs;
  while (!isAtEnd()) {
    defs.push_back(parseDeclarations());
  }
  return make_shared<Program>(loc, std::move(defs));
}

// driver functions
uptr<ASTNode> Parser::parseDeclarations() {
  if (check(DEF)) {
    return parseFuncDef();
  } else if (check(DECL)) {
    return parseFuncDecl();
  } else if (check(CLASS)) {
    return parseClassDef();
  }
  // TODO: global variable
  else if (check(VAR)) {
    return parseVarDef();
  } else {
    throw error(peek(), "Expected 'def', 'class' or 'var' to declare function, class or variable.");
  }
}

uptr<FuncDecl> Parser::parseFuncDecl() {
  Location loc = currentLocation();
  uptr<Header> header = parseHeader();
  if (check(LEFT_BRACE)) {
    throw error(peek(), "Function declaration cannot have a body.");
  }
  return make_unique<FuncDecl>(loc, std::move(header));
}

uptr<FuncDef> Parser::parseFuncDef() {
  Location loc = currentLocation();
  uptr<Header> header = parseHeader();
  consume(LEFT_BRACE, "Expected '{' before function body.");
  vec<uptr<Stmt>> statements;
  while (!check(RIGHT_BRACE) && !isAtEnd()) {
    statements.push_back(parseStmt());
  }
  consume(RIGHT_BRACE, "Expected '}'");
  auto body = make_unique<Block>(currentLocation(), std::move(statements));
  return make_unique<FuncDef>(loc, std::move(header), std::move(body));
}

uptr<ClassDecl> Parser::parseClassDef() {
  auto loc = currentLocation();
  consume(CLASS, "Expected 'class'");
  string class_name = consume(IDENTIFIER, "Expected class name.").lexeme;

  consume(LEFT_BRACE, "Expected '{' before class body.");

  vec<uptr<VarDef>> fields;
  vec<uptr<FuncDef>> methods;

  while (!check(RIGHT_BRACE)) {
    if (check(VAR)) {
      fields.push_back(parseVarDef());
    } else if (check(DEF)) {
      methods.push_back(parseFuncDef());
    } else {
      throw error(peek(), "Expected 'var' or 'def' in class body.");
    }
  }

  consume(RIGHT_BRACE, "Expected '}' after class body.");
  return make_unique<ClassDecl>(loc, class_name, std::move(fields), std::move(methods));
}


uptr<Header> Parser::parseHeader() {
  auto loc = currentLocation();
  if (match({DEF, DECL})) {
    auto token = consume(IDENTIFIER, "Expected function name after 'def' keyword.");
    string func_name = token.lexeme;
    consume(LEFT_PAREN, "Expected '(' after function name.");

    vec<uptr<FuncParameterDecl>> parameters = {};

    if (!check(RIGHT_PAREN)) {
      parameters = parseParameters();
    }
    consume(RIGHT_PAREN, "Expected ')' after function parameters.");

    optional<DataType::DataType> return_type;
    if (match({ARROW})) {
      return_type = parseDataType();
    }
    return make_unique<Header>(loc, std::move(func_name), std::move(return_type), std::move(parameters));
  } else {
    return nullptr;
    error(peek(), "Expected 'def' or 'decl' at the beginning of function declaration.");
  }
}

vec<uptr<FuncParameterDecl>> Parser::parseParameters() {
  vec<uptr<FuncParameterDecl>> params;
  do {
    params.push_back(parseFuncParameterDecl());
  } while (match({COMMA}));
  return params;
}

uptr<FuncParameterDecl> Parser::parseFuncParameterDecl() {
  auto loc = currentLocation();
  vec<string> names;
  Token token = consume(IDENTIFIER, "Expected parameter name.");
  consume(COLON, "expect type after parameter.");
  names.push_back(token.lexeme);
  bool is_ref = match({REF});

  uptr<FuncParameterType> type = parseFuncParameterType(is_ref);
  return make_unique<FuncParameterDecl>(loc, std::move(names), std::move(type));
}

uptr<FuncParameterType> Parser::parseFuncParameterType(bool is_ref) {
  auto loc = currentLocation();
  DataType::DataType base_type = parseDataType();
  vec<optional<int>> dims;
  while (match({LEFT_BRACKET})) {
    if (!check(RIGHT_BRACKET)) {
      Token dim = consume(INTEGEL, "Index should be integel.");
      dims.push_back(std::stoi(dim.lexeme));
    } else {
      dims.push_back(std::nullopt);
    }
    consume(RIGHT_BRACKET, "Expected ']'");
  }
  if (dims.empty()) {
    return make_unique<FuncParameterType>(loc, is_ref, base_type);
  }
  return make_unique<FuncParameterType>(loc, is_ref, base_type, std::move(dims));
}
uptr<VarDef> Parser::parseVarDef() {
  auto loc = currentLocation();
  consume(VAR, "Expected var to delcare variable.");
  vec<string> names;
  Token token = consume(IDENTIFIER, "Expected variable name.");
  names.push_back(token.lexeme);
  while (match({COMMA})) {
    Token else_token = consume(IDENTIFIER, "Expected variable name.");
    names.push_back(else_token.lexeme);
  }
  consume(COLON, "Expected ':' to delcare variable type.");
  auto type = parseType();
  if (type->data_type() == DataType::DataType::MAY_INSTANCE) {
    Token type_tok = consume(IDENTIFIER, "may be an instance of class");
    type->setTypeName(type_tok.lexeme);
  }

  // optional initialization
  optional<uptr<Expr>> init = std::nullopt;
  if (match({EQUAL})) {
    init = parseExpr();
  }

  return make_unique<VarDef>(loc, names, std::move(type), std::move(init));
}
uptr<Type> Parser::parseType() {
  auto loc = currentLocation();
  DataType::DataType base_type = parseDataType();

  vec<optional<int>> dims;
  while (match({LEFT_BRACKET})) {
    if (!check(RIGHT_BRACKET)) {
      Token dim = consume(INTEGEL, "Index should be integel.");
      dims.push_back(std::stoi(dim.lexeme));
    } else {
      dims.push_back(std::nullopt);
    }
    consume(RIGHT_BRACKET, "Expected ']'");
  }

  return make_unique<Type>(loc, base_type, std::move(dims));
}
DataType::DataType Parser::parseDataType() {
  if (match({INT})) return DataType::DataType::INT;
  if (match({BOOL})) return DataType::DataType::BOOL;
  if (match({CHAR})) return DataType::DataType::CHAR;
  if (match({STR})) return DataType::DataType::STRING;
  if (check(IDENTIFIER)) return DataType::DataType::MAY_INSTANCE;
  return DataType::DataType::UNKOWN;
}
uptr<Block> Parser::parseBlock() {
  auto loc = currentLocation();
  consume(LEFT_BRACE, "Expected '{'.");
  vec<uptr<Stmt>> statements;
  while (!check(RIGHT_BRACE) && !isAtEnd()) {
    statements.push_back(parseStmt());
  }
  consume(RIGHT_BRACE, "Expected '}'.");
  return make_unique<Block>(loc, std::move(statements));
}
uptr<Stmt> Parser::parseStmt() {
  auto loc = currentLocation();
  // if (match({NONE})) {
  //     return make_unique<SkipStmt>(loc);// do nothing
  // }
  // if(match({EXIT})){
  //     return make_unique<ExitStmt>(loc);
  // }
  if (match({RETURN})) {
    auto expr = parseExpr();
    return make_unique<ReturnStmt>(loc, std::move(expr));
  }
  if (check(IF)) {
    return parseIfStmt();
  }
  if (check(WHILE)) {
    return parseLoopStmt();
  }
  if (check(DEF)) {
    return parseFuncDef();
  }
  if (check(VAR)) {
    return parseVarDef();
  }
  if (match({BREAK})) {
    std::optional<string> label = std::nullopt;
    if (check(IDENTIFIER)) {
      label = peek().lexeme;
      advance();
    }
    return make_unique<BreakStmt>(loc, label);
  }
  if (match({CONTINUE})) {
    std::optional<string> label = std::nullopt;
    if (check(IDENTIFIER)) {
      label = peek().lexeme;
      advance();
    }
    return make_unique<ContinueStmt>(loc, label);
  }
  // if (match({LEFT_BRACE})) {
  //     return parseBlock();
  // }
  return parseAssignmentOrProcCall();
}
uptr<Stmt> Parser::parseExprStmt() {

  uptr<Stmt> assignStmt = parseAssignmentOrProcCall();
  return assignStmt;
}
uptr<Stmt> Parser::parseAssignmentOrProcCall() {
  auto loc = currentLocation();
  Token token = consume(IDENTIFIER, "Expected identifier.");
  // assigment
  uptr<Lval> left = make_unique<IdLVal>(token.location, token.lexeme);

  // procedure call
  if (match({LEFT_PAREN})) {
    vec<uptr<Expr>> arguments;
    if (!check(RIGHT_PAREN)) {
      arguments = parseArguments();
    }
    consume(RIGHT_PAREN, "Expected ')' after arguments.");
    return make_unique<ProcCall>(loc, token.lexeme, std::move(arguments));
  }
  // handling array index
  while (match({LEFT_BRACKET})) {
    auto index_expr = parseExpr();
    consume(RIGHT_BRACKET, "Expected ']'");
    left = make_unique<IndexLVal>(loc, std::move(left), std::move(index_expr));
  }

  consume(EQUAL, "Expected '=' in assignment statement.");
  uptr<Expr> right = parseExpr();
  return make_unique<AssignStmt>(loc, std::move(left), std::move(right));
}
uptr<IfStmt> Parser::parseIfStmt() {
  Location loc = currentLocation();
  consume(IF, "Expected 'if'");
  consume(LEFT_PAREN, "Expected '('");
  auto cond = parseCond();
  consume(RIGHT_PAREN, "Expected ')'");
  auto then_block = parseBlock();

  vec<std::pair<uptr<Cond>, uptr<Block>>> elifs;
  while (match({ELIF})) {
    consume(LEFT_PAREN, "Expected '(' after elif.");
    auto elif_cond = parseCond();
    consume(RIGHT_PAREN, "Expected ')' after condition.");
    auto elif_block = parseBlock();
    elifs.push_back({std::move(elif_cond), std::move(elif_block)});
  }

  std::optional<uptr<Block>> else_block;
  if (match({ELSE})) {
    else_block = parseBlock();
  }

  return std::make_unique<IfStmt>(loc, std::move(cond), std::move(then_block), std::move(elifs), std::move(else_block));
}
uptr<LoopStmt> Parser::parseLoopStmt() {
  Location loc = currentLocation();
  consume(WHILE, "Expected 'while'");
  consume(LEFT_PAREN, "Expected '('");
  auto cond = parseCond();
  consume(RIGHT_PAREN, "Expected ')'");
  auto body = parseBlock();
  return std::make_unique<LoopStmt>(loc, std::move(cond), std::move(body));
}
uptr<Lval> Parser::parseLVal() {
  Location loc = currentLocation();
  std::unique_ptr<Lval> base;

  if (match({STRING})) {
    base = std::make_unique<StringLiteralLVal>(loc, previous().lexeme);
  } else if (match({IDENTIFIER})) {
    base = std::make_unique<IdLVal>(loc, previous().lexeme);
  } else {
    throw error(peek(), "Expected l-value");
  }

  // Handle suffixes: member access and array indexing
  while (true) {
    // member access: a.b.c
    if (match({DOT})) {
      Token memTok = consume(IDENTIFIER, "Expected member name after '.'");
      auto objExpr = std::make_unique<LValueExpr>(loc, std::move(base));
      base = std::make_unique<MemberAccessLVal>(loc, std::move(objExpr), memTok.lexeme);
      continue;
    }

    // array indexing - [ comes before ] (grammar uses reversed tokens)
    if (match({RIGHT_BRACKET})) {
      auto index = parseExpr();
      consume(LEFT_BRACKET, "Expected ']'");
      base = std::make_unique<IndexLVal>(loc, std::move(base), std::move(index));
      continue;
    }

    break;
  }

  return base;
}
uptr<Expr> Parser::parseExpr() {
  return parseLogicalOr();
}
uptr<Expr> Parser::parseLogicalOr() {
  auto left = parseLogicalAnd();
  while (match({OR})) {
    auto loc = currentLocation();
    auto right = parseLogicalAnd();
    left = make_unique<BinaryExpr>(loc, BinOp::Or, std::move(left), std::move(right));
  }
  return left;
}
uptr<Expr> Parser::parseLogicalAnd() {
  auto left = parseEquality();
  while (match({AND})) {
    Location loc = currentLocation();
    auto right = parseEquality();
    left = std::make_unique<BinaryExpr>(loc, BinOp::And, std::move(left), std::move(right));
  }
  return left;
}
uptr<Expr> Parser::parseEquality() {
  auto left = parseRelational();
  while (match({EQUAL_EQUAL, BANG_EQUAL})) {
    auto loc = currentLocation();
    BinOp op = previous().type == EQUAL_EQUAL ? BinOp::Eq : BinOp::Ne;
    auto right = parseRelational();
    left = make_unique<BinaryExpr>(loc, op, std::move(left), std::move(right));
  }
  return left;
}
uptr<Expr> Parser::parseRelational() {
  auto left = parseAdditive();
  if (match({LESS, GREATER, LESS_EQUAL, GREATER_EQUAL})) {
    Location loc = currentLocation();
    BinOp op;
    Token token = previous();
    switch (token.type) {
      case LESS:
        op = BinOp::Lt;
        break;
      case GREATER:
        op = BinOp::Gt;
        break;
      case LESS_EQUAL:
        op = BinOp::Le;
        break;
      case GREATER_EQUAL:
        op = BinOp::Ge;
        break;
      default:
        throw error(token, "Invalid relational operator.");
    }
    auto right = parseAdditive();
    left = std::make_unique<BinaryExpr>(loc, op, std::move(left), std::move(right));
  }
  return left;
}
uptr<Expr> Parser::parseAdditive() {
  auto left = parseMultiplicative();
  while (true) {
    Location loc = currentLocation();
    BinOp op;
    if (match({PLUS}))
      op = BinOp::Add;
    else if (match({MINUS}))
      op = BinOp::Sub;
    else
      break;

    auto right = parseMultiplicative();
    left = std::make_unique<BinaryExpr>(loc, op, std::move(left), std::move(right));
  }

  return left;
}
uptr<Expr> Parser::parseMultiplicative() {
  auto left = parseUnary();
  while (true) {
    Location loc = currentLocation();
    BinOp op;
    if (match({STAR})) op = BinOp::Mul;
    else if (match({SLASH}))
      op = BinOp::Div;
    else if (match({MODULO}))
      op = BinOp::Mod;
    else
      break;

    auto right = parseUnary();
    left = std::make_unique<BinaryExpr>(loc, op, std::move(left), std::move(right));
  }

  return left;
}
uptr<Expr> Parser::parseUnary() {
  Location loc = currentLocation();

  if (match({PLUS})) {
    return std::make_unique<UnaryExpr>(loc, UnOp::Plus, parseUnary());
  }
  if (match({MINUS})) {
    return std::make_unique<UnaryExpr>(loc, UnOp::Minus, parseUnary());
  }
  if (match({BANG})) {
    return std::make_unique<UnaryExpr>(loc, UnOp::Not, parseUnary());
  }

  return parseCall();
}
uptr<Expr> Parser::parseCall() {
  uptr<Expr> expr = parsePrimary();

  auto loc = currentLocation();
  // function call and method call
  if (match({LEFT_PAREN})) {
    auto lvalExpr = dynamic_cast<LValueExpr *>(expr.get());
    if (lvalExpr) {
      auto idLVal = dynamic_cast<IdLVal *>(lvalExpr->lvalue());
      if (!idLVal) {
        error(peek(), "Expected a callee.");
      }
      auto args = parseArguments();
      consume(RIGHT_PAREN, "Expected ')'");
      expr = std::make_unique<FuncCall>(loc, idLVal->identifier(), std::move(args));
    }

  } else if (match({DOT})) {
    Token memTok = consume(IDENTIFIER, "Expected member name after '.'");
    if (match({LEFT_PAREN})) {
      auto args = parseArguments();
      consume(RIGHT_PAREN, "Expecteded ')'");
      expr = std::make_unique<MethodCall>(loc, std::move(expr), memTok.lexeme, std::move(args));
    } else {
      expr = std::make_unique<MemberAccessExpr>(loc, std::move(expr), memTok.lexeme);
    }
  }
  return expr;
}
uptr<Expr> Parser::parsePrimary() {
  Location loc = currentLocation();

  // Constants
  if (match({INTEGEL})) {
    return std::make_unique<IntConst>(loc, std::stoi(previous().lexeme));
  }
  // if (match({NUMBER})){
  //     return std::make_unique<DoubleConst>(loc, std::stod(previous().lexeme));
  // }
  // if (match({STR})) {
  // }
  if (match({CHAR})) {
    return std::make_unique<CharConst>(loc, previous().lexeme[0]);
  }
  if (match({TRUE})) {
    return std::make_unique<TrueConst>(loc);
  }
  if (match({FALSE})) {
    return std::make_unique<FalseConst>(loc);
  }
  if (match({NEW})) {
    Token clsToken = consume(IDENTIFIER, "Expected a class name to construct instance");
    consume(LEFT_PAREN, "Expected '('");
    vec<uptr<Expr>> args = parseArguments();
    consume(RIGHT_PAREN, "Expected ')'");
    return std::make_unique<NewExpr>(loc, clsToken.lexeme, std::move(args));
  }
  // if (match({SUPER})) {
  //     return std::make_unique<SuperExpr>(loc);
  // }
  // if (match({SELF})) {
  //     return std::make_unique<SelfExpr>(loc);
  // }
  //
  // Parenthesized expression
  if (match({LEFT_PAREN})) {
    auto expr = parseExpr();
    consume(RIGHT_PAREN, "Expected ')'");
    return std::make_unique<ParenExpr>(loc, std::move(expr));
  }
  // array [1,2,3] [[1,2], [1,3]]
  if (match({LEFT_BRACKET})) {
    vec<uptr<Expr>> elems;
    elems.clear();
    do {
      elems.push_back(parseExpr());
    } while (match({COMMA}));
    consume(RIGHT_BRACKET, "Expected ']' at end of array");
    return make_unique<ArrayExpr>(loc, std::move(elems));
  }

  // Identifier-led expressions: variable, function call, member access, method call
  if (check(IDENTIFIER)) {
    Token idTok = advance();
    uptr<Expr> expr;

    // base l-value
    uptr<Lval> lval = std::make_unique<IdLVal>(loc, idTok.lexeme);
    expr = std::make_unique<LValueExpr>(loc, std::move(lval));

    // suffix chain:  [index]
    while (true) {
      // array indexing
      if (match({LEFT_BRACKET})) {
        auto index = parseExpr();
        consume(RIGHT_BRACKET, "Expected ']'");
        if (auto lvalExpr = dynamic_cast<LValueExpr *>(expr.get())) {
          uptr<Lval> lval = lvalExpr->releaseLVal();
          uptr<Lval> indexLval = std::make_unique<IndexLVal>(loc, std::move(lval), std::move(index));
          expr = std::make_unique<LValueExpr>(loc, std::move(indexLval));
        }
      } else {
        break;
      }
    }

    return expr;
  }

  // String literal as l-value
  if (check(STRING)) {
    auto lval = parseLVal();
    return std::make_unique<LValueExpr>(loc, std::move(lval));
  }

  throw error(peek(), "Expected expression");
}
vec<uptr<Expr>> Parser::parseArguments() {
  vec<uptr<Expr>> args;

  if (!check(RIGHT_PAREN)) {
    do {
      args.push_back(parseExpr());
    } while (match({COMMA}));
  }

  return args;
}
uptr<Cond> Parser::parseCond() {
  auto expr = parseExpr();
  return make_unique<ExprCond>(expr->loc, std::move(expr));
}
// uptr<Cond> Parser::parseLogicalOrCond() {
//     auto left = parseLogicalAndCond();

//     while (match({OR})) {
//         Location loc = currentLocation();
//         auto right = parseLogicalAndCond();
//         left = std::make_unique<BinaryCond>(loc, LogicOp::Or, std::move(left), std::move(right));
//     }

//     return left;
// }
// uptr<Cond> Parser::parseLogicalAndCond() {
//     auto left = parseUnaryCond();

//     while (match({AND})) {
//         Location loc = currentLocation();
//         auto right = parseUnaryCond();
//         left = std::make_unique<BinaryCond>(loc, LogicOp::And, std::move(left), std::move(right));
//     }

//     return left;
// }
// uptr<Cond> Parser::parseUnaryCond() {
//     Location loc = currentLocation();

//     if (match({BANG})) {
//         return std::make_unique<NotCond>(loc, parseUnaryCond());
//     }

//     return parsePrimaryCond();
// }
// uptr<Cond> Parser::parsePrimaryCond() {
//     Location loc = currentLocation();

//     if (match({LEFT_PAREN})) {
//         auto cond = parseCond();
//         consume(RIGHT_PAREN, "Expected ')'");
//         return std::make_unique<ParenCond>(loc, std::move(cond));
//     }

//     auto left = parseExpr();

//     return std::make_unique<ExprCond>(loc, std::move(left));
// }

// helper functions...

/// @brief get the previous token
/// @return  previous token
Token Parser::previous() const { return lastToken; }

// ? return the reference of current token?
/// @brief get the current token
/// @return  current token
Token Parser::peek() const { return currentToken; }

/// @brief check if the parser is at the end of tokens
/// @return
bool Parser::isAtEnd() { return peek().type == TOKEN_EOF; }

/// @brief advance the parser
/// @return current token
Token Parser::advance() {
  if (!isAtEnd()) {
    lastToken = currentToken;
    currentToken = scanner.scanToken();
  }
  return previous();
}

/// @brief check if the current token is of a certain type
/// @param type expected toekn type
/// @return
bool Parser::check(TokenType type) {
  if (isAtEnd()) {
    return false;
  }
  return peek().type == type;
}

/// @brief Check if the current token is of any of the given types
/// @param types types list to be checked, one or more
/// @return
bool Parser::match(const initializer_list<TokenType> &types) {
  for (auto type: types) {
    if (check(type)) {
      advance();
      return true;
    }
  }
  return false;
}

/// @brief consume a token and if an error occur, raise parse error
/// @param type expected token type
/// @param message error msg
/// @return
Token Parser::consume(TokenType type, string message) {
  if (check(type))
    return advance();
  // DIE << message;
  auto diag = Diag::getInstance();
  if (diag) {
    diag->report(Diagnostics::Severity::Error, Diagnostics::Phase::Parsing, currentLocation(), message);
  }
  throw std::runtime_error("Parsing failed");
}

Location Parser::currentLocation() const {
  return peek().location;
}

// error handle function and recovery
runtime_error Parser::error(Token token, string message) {
  auto diag = Diag::getInstance();
  if (diag) {
    diag->report(Diagnostics::Severity::Error, Diagnostics::Phase::Parsing, token.location, message);
  }
  throw runtime_error("Parsing failed");
}

void Parser::synchronize() {
  advance();
  while (!isAtEnd()) {
    if (previous().type == SEMICOLON)
      return;
    switch (peek().type) {
      case CLASS:
      case DEF:
      case VAR:
      case FOR:
      case IF:
      case WHILE:
      case RETURN:
      case BREAK:
        return;
      default:
        advance();
    }
  }
}
