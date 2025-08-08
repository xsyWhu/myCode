
#pragma once
#include "token.h"
#include <string>
#include <vector>

class Lexer {
public:
    explicit Lexer(const std::string& input);
    Token nextToken();
    Token peekToken();

private:
    std::string source;
    size_t pos;
    int line, column;
    Token currentToken;

    void skipWhitespaceAndComments();
    char peekChar() const;
    char getChar();
    bool isAtEnd() const;

    Token identifierOrKeyword();
    Token number();
    Token matchOperator();
};
