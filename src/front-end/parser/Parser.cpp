#include <climits>
#include <initializer_list>
#include <iostream>
#include <math.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../include/Expr.hpp"
#include "../include/Logger.hpp"
#include "../include/Parser.hpp"
#include "../include/Stmt.hpp"
#include "../include/Token.hpp"

using std::initializer_list;
using std::runtime_error;
using std::shared_ptr;
using std::to_string;
using std::vector;

vector<shared_ptr<Stmt>> Parser::parse() {
    vector<shared_ptr<Stmt>> statements;
    while (!isAtEnd()) {
        statements.emplace_back(declaration());
    }
    return statements;
}

// main part of a parser ==> Stmt...

/// @brief parser a block
/// @return list of statements
vector<shared_ptr<Stmt>> Parser::block() {
    vector<shared_ptr<Stmt>> statements;

    while (!check(RIGHT_BRACE) && !isAtEnd()) {
        statements.emplace_back(declaration());
    }
    consume(RIGHT_BRACE, "[Cat-Lang] Expect '}' after block.");
    return statements;
}

shared_ptr<Stmt> Parser::declaration() {
    try {
        if (match({CLASS}))
            return classDeclaration();
        if (match({DEF}))
            return function("function");
        if (match({VAR}))
            return varDeclaration();
        return statement();
    } catch (runtime_error const &error) {
        synchronize();
        return nullptr;
    }
}

shared_ptr<VarType> Parser::parseVarType() {
    auto varType = std::make_shared<VarType>();
    if (match({INT, DOUBLE, BOOL, STR, IDENTIFIER})) {
        varType->name = previous().lexeme;
        return varType;
    }
    if (match({LIST})) {// support nested list
        varType->name = previous().lexeme;
        consume(LESS, "[Cat-Lang] Expect '<' after list.");
        varType->generics.push_back(parseVarType());
        consume(GREATER, "[Cat-Lang] Expect '>' after list.");
    } else {
        error(peek(), "[Cat-Lang] Expect variable type.");
    }
    return varType;
}

/// @brief parsre a variable declaration, like var a:int;
/// @return var Stmt Node, one parsed data
shared_ptr<Stmt> Parser::varDeclaration() {
    Token identifier = consume(IDENTIFIER, "[Cat-Lang] Expect variable.");
    Token colon = consume(COLON, "[Cat-Lang] Expect ':' after variable.");
    string typeName{""};
    auto type = parseVarType();
    // convert it back to string
    if (type->generics.size() == 0) {
        typeName = type->name;
    } else {
        auto inner = type->generics;
        typeName = type->name;
        int nested{0};
        while (inner.size() != 0) {
            typeName = typeName + "<" + inner[0]->name;
            inner = inner[0]->generics;
            nested++;
        }
        while (nested--) {
            typeName += ">";
        }
    }
    shared_ptr<Expr<Object>> initializer =
        match({EQUAL}) ? expression() : nullptr;
    consume(SEMICOLON, "[Cat-Lang] Expect ';' after variable declaration.");
    shared_ptr<Stmt> var = std::make_shared<Var>(identifier, initializer, typeName);
    return var;
}

/// @brief parse the function declaration
/// @param kind the kind of function
/// @return
shared_ptr<Function> Parser::function(string kind) {
    Token identifier =
        consume(IDENTIFIER, "[Cat-Lang] Expect " + kind + ".");
    consume(LEFT_PAREN, "[Cat-Lang] Expect '(' after " + kind + ".");
    vector<std::pair<Token, string>> parameters;
    bool isFirst = true;
    // handle the parameters like (a, b) or (a:int, b:int)
    if (!check(RIGHT_PAREN)) {
        do {
            if (parameters.size() >= 255) {
                error(peek(), "[Cat-Lang] Cannot have more than 255 parameters.");
            }
            if (kind == "method" && isFirst) {
                auto self = consume(SELF, "[Cat-Lang] Expect self in method.");
                parameters.emplace_back(self, "");
                isFirst = false;
                continue;
            }
            Token name = consume(IDENTIFIER, "[Cat-Lang] Expect non-keyword parameter.");
            string typeName("");
            consume(COLON, "[Cat-Lang] Except : after parameter.");
            if (match({INT, DOUBLE, BOOL, STR, LIST, IDENTIFIER})) {
                typeName = previous().lexeme;
                if (previous().type == LIST) {
                    consume(LESS, "[Cat-Lang] Expect '<' after list.");
                    if (match({INT, DOUBLE, BOOL, STR, IDENTIFIER})) {
                        typeName += "<" + previous().lexeme + ">";
                        consume(GREATER, "[Cat-Lang] Expect '>' after list.");
                    } else {
                        error(peek(), "[Cat-Lang] Expect list element type.");
                    }
                }
            } else {
                error(peek(), "[Cat-Lang] Expect parameter type.");
            }
            parameters.emplace_back(name, typeName);
        } while (match({COMMA}));
    }
    consume(RIGHT_PAREN, "[Cat-Lang] Expect ')' after parameters.");

    Token returnType;
    if (match({ARROW})) {
        if (match({INT, DOUBLE, BOOL, STR, LIST, IDENTIFIER})) {
            returnType = previous();
            if (previous().type == LIST) {
                consume(LESS, "[Cat-Lang] Expect '<' after list.");
                if (match({INT, DOUBLE, BOOL, STR, IDENTIFIER})) {
                    returnType.lexeme += "<" + previous().lexeme + ">";
                    consume(GREATER, "[Cat-Lang] Expect '>' after list.");
                } else {
                    error(peek(), "[Cat-Lang] Expect list element type.");
                }
            }
        } else {
            Error::addWarn(peek(), "[Cat-Lang] Expect function return value type.");
        }
        // returnType = consume(IDENTIFIER, "[Cat-Lang] Expect function return value type.");
    } else {
        Error::addWarn(peek(), "[Cat-Lang] Warn! function return int type.");
    }
    consume(LEFT_BRACE, "[Cat-Lang] Expect '{' before " + kind + " body.");
    auto body = block();
    auto func = std::make_shared<Function>(identifier, parameters, body, returnType);
    return func;
}

/// @brief parse the class declaration
/// @return
shared_ptr<Stmt> Parser::classDeclaration() {
    Token identifier = consume(IDENTIFIER, "[Cat-Lang] Expect class.");

    shared_ptr<Variable<Object>> superclass;
    if (match({LESS})) {
        consume(IDENTIFIER, "[Cat-Lang] Expect superclass.");
        superclass = std::make_shared<Variable<Object>>(previous());
    }
    consume(LEFT_BRACE, "[Cat-Lang] Expect '{' before class body.");

    vector<shared_ptr<Function>> methods;
    vector<shared_ptr<Stmt>> classStatements;

    while (!check(RIGHT_BRACE) && !isAtEnd()) {
        while (match({VAR})) {
            classStatements.push_back(std::dynamic_pointer_cast<Var>(varDeclaration()));
        }
        methods.push_back(function("method"));
        classStatements.push_back(methods.back());
    }
    auto body = std::make_shared<Block>(classStatements);
    consume(RIGHT_BRACE, "[Cat-Lang] Expect '}' after class body.");

    auto class_decl = std::make_shared<Class>(identifier, superclass, methods, body);
    return class_decl;
}

shared_ptr<Stmt> Parser::statement() {
    if (match({FOR}))
        return forStatement();
    if (match({IF}))
        return ifStatement();
    if (match({PRINT}))
        return printStatement();
    if (match({RETURN}))
        return returnStatement();
    if (match({WHILE}))
        return whileStatement();
    // TODO
    // if (match({BREAK, CONTINUE}))
    //     return controlStatement();
    // if (match({TRY}))
    //     return tryStatement();
    // if (match({THROW}))
    //     return throwStatement();
    if (match({LEFT_BRACE}))
        return std::make_shared<Block>(block());
    return expressionStatement();
}

/// @brief parse an expression statement
/// @return
shared_ptr<Stmt> Parser::expressionStatement() {
    shared_ptr<Expr<Object>> expr = expression();
    consume(SEMICOLON, "[Cat-Lang] Expect ';' after expression.");
    return std::make_shared<Expression>(expr);
}

/// @brief parse a for statement, like for(...)
/// @return
shared_ptr<Stmt> Parser::forStatement() {
    consume(LEFT_PAREN, "[Cat-Lang] Expect '(' after 'for'.");
    shared_ptr<Stmt> initializer;
    if (match({SEMICOLON}))
        initializer = nullptr;
    else if (match({VAR}))
        initializer = varDeclaration();
    else
        initializer = expressionStatement();

    shared_ptr<Expr<Object>> condition =
        !check(SEMICOLON) ? assignment() : nullptr;
    consume(SEMICOLON, "[Cat-Lang] Expect ';' after loop condition.");

    shared_ptr<Expr<Object>> increment =
        !check(RIGHT_PAREN) ? assignment() : nullptr;
    consume(RIGHT_PAREN, "[Cat-Lang] Expect ')' after for clauses.");

    shared_ptr<Stmt> body = statement();

    if (increment != nullptr) {
        vector<shared_ptr<Stmt>> stmts;
        stmts.emplace_back(body);
        stmts.emplace_back(std::make_shared<Expression>(increment));
        // stmts.emplace_back(shared_ptr<Stmt>(new Expression(increment)));
        body = std::make_shared<Block>(stmts);
    }

    if (condition == nullptr) {
        condition = std::make_shared<Literal<Object>>(Object::make_obj(true));
    }
    body = std::make_shared<While>(condition, body);

    if (initializer != nullptr) {
        vector<shared_ptr<Stmt>> stmts2;
        stmts2.emplace_back(initializer);
        stmts2.emplace_back(body);
        body = std::make_shared<Block>(stmts2);
    }

    return body;
}

/// @brief parse a if statement, like if(...)
/// @return
shared_ptr<Stmt> Parser::ifStatement() {
    consume(LEFT_PAREN, "[Cat-Lang] Expect '(' after 'if'.");
    shared_ptr<Expr<Object>> if_condition = expression();
    consume(RIGHT_PAREN, "[Cat-Lang] Expect ')' after if condition.");
    shared_ptr<Stmt> then_branch = statement();
    IfBranch main_brach{std::move(if_condition), std::move(then_branch)};
    vector<IfBranch> elif_branches;
    while (match({ELIF})) {
        consume(LEFT_PAREN, "[Cat-Lang] Expect '(' after 'elif'.");
        auto elif_condition = assignment();
        consume(RIGHT_PAREN, "[Cat-Lang] Expect ')' after elif condition.");
        auto elif_branch = statement();
        elif_branches.emplace_back(std::move(elif_condition), std::move(elif_branch));
    }
    auto else_branch = match({ELSE}) ? statement() : nullptr;

    return std::make_shared<If>(main_brach, elif_branches, else_branch);
    // return shared_ptr<Stmt>(new If(main_brach, elif_branches, else_branch));
}

/// @brief parse a print statement, like print(...)
/// @return
shared_ptr<Stmt> Parser::printStatement() {
    // ! deperate usage, this didn't view print as a function
    // shared_ptr<Expr<Object>> value = expression();
    // consume(SEMICOLON, "Expect ';' after value.");
    // shared_ptr<Stmt> print(new Print(value));
    // return print;
    Token identifier = previous();
    if (!match({LEFT_PAREN})) {
        throw error(identifier, "[Cat-Lang] Expect '(' after 'print'.");
    }
    auto expr = finishCall(std::make_shared<Variable<Object>>(identifier));
    consume(SEMICOLON, "[Cat-Lang] Expect ';' after value.");
    return std::make_shared<Print>(expr);
}

/// @brief parse a return statement
/// @return
shared_ptr<Stmt> Parser::returnStatement() {
    Token keyword = previous();
    auto value = check(TokenType::SEMICOLON) ? nullptr : expression();
    consume(SEMICOLON, "[Cat-Lang] Expect ';' after return value.");
    return std::make_shared<Return>(keyword, value);
}

/// @brief parse a while statement, like while(...)
/// @return
shared_ptr<Stmt> Parser::whileStatement() {
    consume(LEFT_PAREN, "[Cat-Lang] Expect '(' after 'while'.");
    shared_ptr<Expr<Object>> condition = expression();
    consume(RIGHT_PAREN, "[Cat-Lang] Expect ')' after condition.");
    shared_ptr<Stmt> body = statement();
    return std::make_shared<While>(condition, body);
}

/// @brief parse a control statement, like {breal; continue}
/// @return
// shared_ptr<Stmt> Parser::controlStatement() {
//     Token keyword = previous();
//     if (keyword.type == BREAK) {
//         consume(SEMICOLON, "[Cat-Lang] [Cat-Lang] Expect ';' after 'break'.");
//         return std::make_shared<Break>(keyword);
//     } else if (keyword.type == CONTINUE) {
//         consume(SEMICOLON, "[Cat-Lang] [Cat-Lang] Expect ';' after 'continue'.");
//         return std::make_shared<Continue>(keyword);
//     }
//     return nullptr;
// }

/// @brief parse a try statement, like try{...}
/// @return
// shared_ptr<Stmt> Parser::tryStatement() {
//     // TODO
//     return nullptr;
// }

/// @brief parse a throw statement, like throw ...
/// @return
// shared_ptr<Stmt> Parser::throwStatement() {
//     // TODO
//     return nullptr;
// }

// ==> Expr...

/// @brief parse the expressions
/// @return
shared_ptr<Expr<Object>> Parser::expression() { return assignment(); }

/// @brief parse the assignment, like a=1
/// @return
shared_ptr<Expr<Object>> Parser::assignment() {
    shared_ptr<Expr<Object>> expr = orExpression();
    if (match({EQUAL})) {
        Token equals = previous();
        shared_ptr<Expr<Object>> value = assignment();

        if (auto subscript = dynamic_pointer_cast<Subscript<Object>>(expr);
            subscript != nullptr) {
            Token name = subscript->identifier;
            return std::make_shared<Subscript<Object>>(
                std::move(name), subscript->index, std::move(value)
            );
        }
        if (auto variable = dynamic_pointer_cast<Variable<Object>>(expr);
            variable != nullptr) {
            Token name = variable->name;
            return std::make_shared<Assign<Object>>(name, value);
        }
        if (auto get = dynamic_pointer_cast<Get<Object>>(expr); get != nullptr) {
            return std::make_shared<Set<Object>>(get->object, get->name, value);
        }
        error(equals, "[Cat-Lang] Invalid assignment target.");
    }
    return expr;
}

/// @brief parse the or expression, like 1 or 2
/// @return
shared_ptr<Expr<Object>> Parser::orExpression() {
    shared_ptr<Expr<Object>> expr = andExpression();
    while (match({OR})) {
        Token operation = previous();
        shared_ptr<Expr<Object>> right = andExpression();
        expr = std::make_shared<Logical<Object>>(expr, operation, right);
    }
    return expr;
}

/// @brief parse the and expression, like 1 and 2
/// @return
shared_ptr<Expr<Object>> Parser::andExpression() {
    shared_ptr<Expr<Object>> expr = equality();
    while (match({AND})) {
        Token operation = previous();
        shared_ptr<Expr<Object>> right = equality();
        expr = std::make_shared<Logical<Object>>(expr, operation, right);
    }
    return expr;
}

template<typename Fn>
shared_ptr<Expr<Object>>
Parser::binary(Fn func, const std::initializer_list<TokenType> &token_args) {
    auto expr = func();
    while (match(token_args)) {
        auto op = previous();
        auto right = func();
        expr = std::make_shared<Binary<Object>>(expr, op, right);
    }
    return expr;
}

/// @brief parse an equality expreeion, like a!=b, a==b
/// @return
shared_ptr<Expr<Object>> Parser::equality() {
    auto expr =
        binary([this]() { return comparison(); }, {BANG_EQUAL, EQUAL_EQUAL});
    return expr;
}

/// @brief parse a comparsion expreesion, like a<b, a<=b, a>b, a>=b
/// @return
shared_ptr<Expr<Object>> Parser::comparison() {
    auto expr = binary([this]() { return term(); }, {TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL});
    return expr;
}

/// @brief parse a term expression, like a+b, a-b
/// @return
shared_ptr<Expr<Object>> Parser::term() {
    auto expr = binary([this]() { return factor(); }, {TokenType::MINUS, TokenType::PLUS});
    return expr;
}

/// @brief parse factor expreesion, like a^b, a/b, a*b, a%b
/// @return
shared_ptr<Expr<Object>> Parser::factor() {
    auto expr = binary([this]() { return unary(); }, {TokenType::SLASH, TokenType::BACKSLASH, TokenType::STAR, TokenType::MODULO});
    return expr;
}

shared_ptr<Expr<Object>> Parser::unary() {
    if (match({BANG, MINUS})) {
        Token operation = previous();
        shared_ptr<Expr<Object>> right = unary();
        return std::make_shared<Unary<Object>>(operation, right);
    }
    // return prefix();
    return call();
}

/// @brief parse a prefix expreesion, like ++a, --a
/// @return
// shared_ptr<Expr<Object>> Parser::prefix() {
//     if (match({PLUS_PLUS, MINUS_MINUS})) {
//         const auto op = previous();
//         auto lvalue = consume(IDENTIFIER, "[Cat-Lang] Operators '++' and '--' "
//                                           "must be applied to and lvalue operand.");

//         if (lvalue.type == PLUS_PLUS || lvalue.type == MINUS_MINUS) {
//             throw error(
//                 peek(),
//                 "[Cat-Lang] Operators '++' and '--' cannot be concatenated."
//             );
//         }

//         if (op.type == PLUS_PLUS) {
//             return std::make_shared<Increment<Object>>(
//                 lvalue, Increment<Object>::Type::PREFIX
//             );
//         } else {
//             return std::make_shared<Decrement<Object>>(
//                 lvalue, Decrement<Object>::Type::PREFIX
//             );
//         }
//     }

//     return postfix();
// }
/// @brief parse a postfix expression, like a++, a--. etc.
/// @return
// shared_ptr<Expr<Object>> Parser::postfix() {
//     auto expr = call();

//     if (match({TokenType::PLUS_PLUS, TokenType::MINUS_MINUS})) {
//         const auto op = previous();
//         // Forbid incrementing/decrementing rvalues.
//         if (!dynamic_cast<Variable<Object> *>(expr.get())) {
//             throw error(op, "[Cat-Lang] Operators '++' and '--' must be applied "
//                             "to and lvalue operand.");
//         }

//         // Concatenating increment/decrement operators is not allowed.
//         if (match({TokenType::PLUS_PLUS, TokenType::MINUS_MINUS})) {
//             throw error(
//                 op, "[Cat-Lang] Operators '++' and '--' cannot be concatenated."
//             );
//         }

//         if (op.type == TokenType::PLUS_PLUS) {
//             expr = std::make_shared<Increment<Object>>(
//                 dynamic_cast<Variable<Object> *>(expr.get())->name,
//                 Increment<Object>::Type::POSTFIX
//             );
//         } else {
//             expr = std::make_shared<Decrement<Object>>(
//                 dynamic_cast<Variable<Object> *>(expr.get())->name,
//                 Decrement<Object>::Type::POSTFIX
//             );
//         }
//     }

//     return expr;
// }

/// @brief parse a call expression, like fub()
/// @return
shared_ptr<Expr<Object>> Parser::call() {
    shared_ptr<Expr<Object>> expr = subscript();
    while (true) {
        if (match({LEFT_PAREN})) {
            expr = finishCall(expr);
        } else if (match({DOT})) {
            Token name =
                consume(IDENTIFIER, "[Cat-Lang] Expect property name after '.'.");
            expr = std::make_shared<Get<Object>>(expr, name);
        } else {
            break;
        }
    }
    return expr;
}

/// @brief finish parsing a call exapression
/// @param expr callee
/// @return
shared_ptr<Expr<Object>> Parser::finishCall(shared_ptr<Expr<Object>> callee) {
    vector<shared_ptr<Expr<Object>>> arguments;
    if (!check(RIGHT_PAREN)) {
        do {
            if (arguments.size() >= 255) {
                error(peek(), "[Cat-Lang] Cannot have more than 255 arguments.");
            }
            arguments.push_back(expression());
        } while (match({COMMA}));
    }

    Token paren =
        consume(RIGHT_PAREN, "[Cat-Lang] Expect ')' after arguments.");

    return std::make_shared<Call<Object>>(callee, paren, arguments);
}

/// @brief parse a subscript expression, like a[][]
/// @return
shared_ptr<Expr<Object>> Parser::subscript() {
    shared_ptr<Expr<Object>> expr = primary();
    while (true) {
        if (match({LEFT_BRACKET})) {
            expr = finishSubscript(expr);
        } else {
            break;
        }
    }
    return expr;
}

/// @brief finih parsing, that is implement
/// @param expr
/// @return
shared_ptr<Expr<Object>>
Parser::finishSubscript(shared_ptr<Expr<Object>> identifier) {
    auto index = orExpression();
    consume(TokenType::RIGHT_BRACKET, "[Cat-Lang] Expect ']' after arguments.");

    // Forbid calling rvalues.
    if (!dynamic_cast<Variable<Object> *>(identifier.get())) {
        throw error(peek(), "[Cat-Lang] rvalue is not subscriptable.");
    }

    auto var = dynamic_cast<Variable<Object> *>(identifier.get())->name;

    return std::make_shared<Subscript<Object>>(var, index, nullptr);
}

/// @brief parse the lambda function
/// @return
// shared_ptr<Expr<Object>> Parser::lambda() {
//     return nullptr;
// }

/// @brief parse a primary expression
/// @return
shared_ptr<Expr<Object>> Parser::primary() {
    if (match({FALSE})) {
        return std::make_shared<Literal<Object>>(Object::make_obj(false));
    }
    if (match({TRUE})) {
        return std::make_shared<Literal<Object>>(Object::make_obj(true));
    }
    if (match({NONE})) {
        return std::make_shared<Literal<Object>>(Object::make_none_obj());
    }

    if (match({NUMBER})) {
        return std::make_shared<Literal<Object>>(
            Object::make_obj(std::get<double>(previous().literal.data))
        );
    }
    if (match({INTEGEL})) {
        return std::make_shared<Literal<Object>>(
            Object::make_obj(std::get<int>(previous().literal.data))
        );
    }
    if (match({STRING})) {
        return std::make_shared<Literal<Object>>(
            Object::make_obj(std::get<std::string>(previous().literal.data))
        );
    }

    if (match({SUPER})) {
        // find token lexeme is class
        // get the class name
        int c_cur = current;
        while (tokens[c_cur].lexeme != "class") {
            c_cur--;
        }
        Token className = tokens[c_cur + 1];
        Token keyword = previous();
        consume(DOT, "[Cat-Lang] Expect '.' after 'super'.");
        Token method =
            consume(IDENTIFIER, "[Cat-Lang] Expect superclass method name.");
        return std::make_shared<Super<Object>>(keyword, className, method);
    }

    if (match({SELF})) {
        return std::make_shared<Self<Object>>(previous());
    }

    if (match({IDENTIFIER})) {
        return std::make_shared<Variable<Object>>(previous());
    }

    if (match({LEFT_PAREN})) {
        shared_ptr<Expr<Object>> expr = expression();
        consume(RIGHT_PAREN, "[Cat-Lang] Expect ')' after expression.");
        return std::make_shared<Grouping<Object>>(expr);
    }

    if (match({LEFT_BRACKET})) {
        Token opening_bracket = previous();
        auto expr = list();
        consume(RIGHT_BRACKET, "[Cat-Lang] Expect ']' at the end of a list");
        return std::make_shared<List<Object>>(opening_bracket, expr);
    }
    throw error(peek(), "[Cat-Lang] Expect expression.");
}

/// @brief parse a list expression, a = []
/// @return
vector<shared_ptr<Expr<Object>>> Parser::list() {
    vector<shared_ptr<Expr<Object>>> items;
    // Return an empty list if there are no values.
    if (check(RIGHT_BRACKET))
        return items;
    do {
        if (check(RIGHT_BRACKET))
            break;

        items.emplace_back(orExpression());

        if (items.size() > 100)
            error(peek(), "[Cat-Lang] Cannot have more than 100 items in a list.");

    } while (match({COMMA}));

    return items;
}

// helper functions...

/// @brief get the previous token
/// @return  previous token
Token Parser::previous() { return tokens[current - 1]; }

// ? return the reference of current token?
/// @brief get the current token
/// @return  current token
Token Parser::peek() { return tokens[current]; }

/// @brief check if the parser is at the end of tokens
/// @return
bool Parser::isAtEnd() { return peek().type == TOKEN_EOF; }

/// @brief advance the parser
/// @return current token
Token Parser::advance() {
    if (!isAtEnd()) {
        current++;
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
    throw error(peek(), message);
}

// error handle function and recovery
runtime_error Parser::error(Token token, string message) {
    Error::addError(token, std::move(message));
    if (token.type == TOKEN_EOF) {
        return runtime_error(to_string(token.line) + " at end" + message);
    } else {
        return runtime_error(to_string(token.line) + " at '" + token.lexeme + "'" + message);
    }
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
            case PRINT:
            case RETURN:
            case BREAK:
                return;
            default:
                advance();
        }
    }
}
