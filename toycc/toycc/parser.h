
#pragma once
#include "lexer.h"
#include "ast.h"

class Parser {
public:
    explicit Parser(Lexer& lexer);
    std::vector<std::shared_ptr<FuncDef>> parseCompUnit();

private:
    Lexer& lexer;
    Token current;

    void advance();
    bool match(TokenType type);
    bool check(TokenType type) const;
    Token expect(TokenType type, const std::string& msg);

    std::shared_ptr<FuncDef> parseFuncDef();
    std::vector<Param> parseParamList();
    std::shared_ptr<BlockStmt> parseBlock();
    std::shared_ptr<Stmt> parseStmt();
    std::shared_ptr<Expr> parseExpr();
    std::shared_ptr<Expr> parseLOrExpr();
    std::shared_ptr<Expr> parseLAndExpr();
    std::shared_ptr<Expr> parseRelExpr();
    std::shared_ptr<Expr> parseAddExpr();
    std::shared_ptr<Expr> parseMulExpr();
    std::shared_ptr<Expr> parseUnaryExpr();
    std::shared_ptr<Expr> parsePrimaryExpr();
};
