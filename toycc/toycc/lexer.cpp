#include "lexer.h"
#include <cctype>
#include <stdexcept>
#include <iostream>

Lexer::Lexer(const std::string& input)
    : source(input), pos(0), line(1), column(1) {}

char Lexer::peekChar() const {
    return pos < source.size() ? source[pos] : '\0';
}

char Lexer::getChar() {
    if (isAtEnd()) return '\0';
    char c = source[pos++];
    if (c == '\n') {
        ++line;
        column = 1;
    }
    else {
        ++column;
    }
    return c;
}

bool Lexer::isAtEnd() const {
    return pos >= source.size();
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peekChar();
        if (isspace(c)) {
            getChar();
        }
        else if (c == '/' && source[pos + 1] == '/') {
            while (!isAtEnd() && peekChar() != '\n') getChar();
        }
        else if (c == '/' && source[pos + 1] == '*') {
            getChar(); getChar();
            while (!isAtEnd()) {
                if (peekChar() == '*' && source[pos + 1] == '/') {
                    getChar(); getChar();
                    break;
                }
                getChar();
            }
        }
        else {
            break;
        }
    }
}

Token Lexer::identifierOrKeyword() {
    int startCol = column;
    std::string lexeme;
    while (isalnum(peekChar()) || peekChar() == '_') {
        lexeme += getChar();
    }
    if (lexeme == "int")      return Token(TokenType::INT, lexeme, line, startCol);
    if (lexeme == "void")     return Token(TokenType::VOID, lexeme, line, startCol);
    if (lexeme == "return")   return Token(TokenType::RETURN, lexeme, line, startCol);
    if (lexeme == "if")       return Token(TokenType::IF, lexeme, line, startCol);
    if (lexeme == "else")     return Token(TokenType::ELSE, lexeme, line, startCol);
    if (lexeme == "while")    return Token(TokenType::WHILE, lexeme, line, startCol);
    if (lexeme == "break")    return Token(TokenType::BREAK, lexeme, line, startCol);
    if (lexeme == "continue") return Token(TokenType::CONTINUE, lexeme, line, startCol);
    return Token(TokenType::IDENTIFIER, lexeme, line, startCol);
}

Token Lexer::number() {
    int startCol = column;
    std::string lexeme;
    while (isdigit(peekChar())) {
        lexeme += getChar();
    }
    return Token(TokenType::NUMBER, lexeme, line, startCol);
}

Token Lexer::matchOperator() {
    int startCol = column;
    char c = getChar();
    switch (c) {
    case '+': return Token(TokenType::PLUS, "+", line, startCol);
    case '-': return Token(TokenType::MINUS, "-", line, startCol);
    case '*': return Token(TokenType::MULT, "*", line, startCol);
    case '/': return Token(TokenType::DIV, "/", line, startCol);
    case '%': return Token(TokenType::MOD, "%", line, startCol);
    case '=': return peekChar() == '=' ? (getChar(), Token(TokenType::EQ, "==", line, startCol)) : Token(TokenType::ASSIGN, "=", line, startCol);
    case '!': return peekChar() == '=' ? (getChar(), Token(TokenType::NE, "!=", line, startCol)) : Token(TokenType::NOT, "!", line, startCol);
    case '<': return peekChar() == '=' ? (getChar(), Token(TokenType::LE, "<=", line, startCol)) : Token(TokenType::LT, "<", line, startCol);
    case '>': return peekChar() == '=' ? (getChar(), Token(TokenType::GE, ">=", line, startCol)) : Token(TokenType::GT, ">", line, startCol);
    case '&': if (peekChar() == '&') return getChar(), Token(TokenType::AND, "&&", line, startCol); break;
    case '|': if (peekChar() == '|') return getChar(), Token(TokenType::OR, "||", line, startCol); break;
    case ';': return Token(TokenType::SEMICOLON, ";", line, startCol);
    case ',': return Token(TokenType::COMMA, ",", line, startCol);
    case '(': return Token(TokenType::LPAREN, "(", line, startCol);
    case ')': return Token(TokenType::RPAREN, ")", line, startCol);
    case '{': return Token(TokenType::LBRACE, "{", line, startCol);
    case '}': return Token(TokenType::RBRACE, "}", line, startCol);
    }
    return Token(TokenType::UNKNOWN, std::string(1, c), line, startCol);
}

Token Lexer::nextToken() {
    skipWhitespaceAndComments();
    if (isAtEnd()) {
        Token tok(TokenType::END_OF_FILE, "", line, column);
        std::cout << "[LEXER] Token: " << static_cast<int>(tok.type) << " Lexeme: " << tok.lexeme << std::endl;
        return tok;
    }
    char c = peekChar();
    Token tok;
    if (isalpha(c) || c == '_') {
        tok = identifierOrKeyword();
    } else if (isdigit(c)) {
        tok = number();
    } else {
        tok = matchOperator();
    }
    std::cout << "[LEXER] Token: " << static_cast<int>(tok.type) << " Lexeme: " << tok.lexeme << std::endl;
    return tok;
}

Token Lexer::peekToken() {
    size_t backupPos = pos;
    int backupLine = line, backupCol = column;
    Token tok = nextToken();
    pos = backupPos; line = backupLine; column = backupCol;
    return tok;
}
