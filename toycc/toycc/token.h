#pragma once
#include <string>

enum class TokenType {
    INT, VOID, RETURN, IF, ELSE, WHILE, BREAK, CONTINUE,
    IDENTIFIER, NUMBER,
    PLUS, MINUS, MULT, DIV, MOD,
    LT, GT, LE, GE, EQ, NE,
    AND, OR, NOT, ASSIGN,
    SEMICOLON, COMMA, LPAREN, RPAREN, LBRACE, RBRACE,
    END_OF_FILE, UNKNOWN
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;

    Token() : type(TokenType::UNKNOWN), lexeme(""), line(0), column(0) {}

    Token(TokenType t, const std::string& l, int ln, int col)
        : type(t), lexeme(l), line(ln), column(col) {}
};
