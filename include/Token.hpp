#ifndef TOKEN_HPP_
#define TOKEN_HPP_
#include "Location.hpp"
#include <string>
using std::string;
using std::to_string;
typedef enum {
    LEFT_PAREN, // (
    RIGHT_PAREN,// )
    LEFT_BRACE, // {
    RIGHT_BRACE,// }
    COMMA,      // ,
    DOT,        // .
    MINUS,      //-
    PLUS,       //=
    CARAT,      // ^
    SEMICOLON,  // ;
    SLASH,      // /
    STAR,       // *
    BACKSLASH,  // (\)
    MODULO,     // %
    COLON,      // :

    // One or two character tokens.
    RIGHT_BRACKET,// ]
    LEFT_BRACKET, // [
    BANG,         // !
    BANG_EQUAL,   // !=
    EQUAL,        // =
    EQUAL_EQUAL,  //==
    GREATER,      // >
    GREATER_EQUAL,//>=
    LESS,         //<
    LESS_EQUAL,   //<=
    ARROW,        // ->
    MINUS_MINUS,  //--
    PLUS_PLUS,    //++


    // Literals.
    IDENTIFIER,
    STRING,
    INTEGEL,
    NUMBER,

    // Keywords.
    // logic operators
    AND,
    OR,
    TRUE,
    FALSE,
    // branch
    IF,
    ELSE,
    ELIF,
    // declare
    DEF,
    VAR,
    // return
    RETURN,
    // none
    NONE,
    // refernce
    REF,
    // class
    CLASS,
    SUPER,
    SELF,
    // loop
    WHILE,
    FOR,
    // control flow
    BREAK,
    CONTINUE,
    // type
    INT,
    DOUBLE,
    BOOL,
    STR,
    LIST,

    // TODO
    LAMBDA,
    TRY,
    THROW,

    TOKEN_EOF
} TokenType;

class Token {
public:
    Token()
        : type(TokenType::NONE), lexeme(""), location(-1, -1) {}
    Token(TokenType type, string lexeme, int line, int column = -1)
        : type(type), lexeme(lexeme), location(line, column) {}

    string toString() {
        auto msg = to_string(type) + " lexeme: '" + lexeme + "'";
        return msg;
    }
    TokenType type;
    string lexeme;
    Location location;
};

#endif// TOKEN_HPP_
