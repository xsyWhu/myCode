#include "parser.h"
#include <stdexcept>

Parser::Parser(Lexer& lexer) : lexer(lexer) {
    advance();
}

void Parser::advance() {
    current = lexer.nextToken();
}

bool Parser::match(TokenType type) {
    if (current.type == type) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenType type) const {
    return current.type == type;
}

Token Parser::expect(TokenType type, const std::string& msg) {
    if (current.type != type) {
        throw std::runtime_error("Parser error: expected " + msg);
    }
    Token t = current;
    advance();
    return t;
}

std::vector<std::shared_ptr<FuncDef>> Parser::parseCompUnit() {
    std::vector<std::shared_ptr<FuncDef>> functions;
    while (!check(TokenType::END_OF_FILE)) {
        functions.push_back(parseFuncDef());
    }
    return functions;
}

std::shared_ptr<FuncDef> Parser::parseFuncDef() {
    std::string retType;
    if (match(TokenType::INT)) retType = "int";
    else if (match(TokenType::VOID)) retType = "void";
    else throw std::runtime_error("Expected 'int' or 'void' at function return type");

    std::string funcName = expect(TokenType::IDENTIFIER, "function name").lexeme;
    expect(TokenType::LPAREN, "(");

    std::vector<Param> params;
    if (!check(TokenType::RPAREN)) {
        params = parseParamList();
    }

    expect(TokenType::RPAREN, ")");
    auto body = parseBlock();

    auto func = std::make_shared<FuncDef>();
    func->retType = retType;
    func->name = funcName;
    func->params = params;
    func->body = body;
    return func;
}

std::vector<Param> Parser::parseParamList() {
    std::vector<Param> params;
    while (true) {
        expect(TokenType::INT, "'int' for parameter");
        std::string name = expect(TokenType::IDENTIFIER, "parameter name").lexeme;
        params.push_back(Param{ name });
        if (!match(TokenType::COMMA)) break;
    }
    return params;
}

std::shared_ptr<BlockStmt> Parser::parseBlock() {
    expect(TokenType::LBRACE, "{");
    auto block = std::make_shared<BlockStmt>();
    while (!check(TokenType::RBRACE)) {
        block->statements.push_back(parseStmt());
    }
    expect(TokenType::RBRACE, "}");
    return block;
}

std::shared_ptr<Stmt> Parser::parseStmt() {
    if (check(TokenType::LBRACE)) {
        return parseBlock();
    }
    if (match(TokenType::SEMICOLON)) {
        return std::make_shared<ExprStmt>(nullptr);
    }
    if (match(TokenType::INT)) {
        std::string name = expect(TokenType::IDENTIFIER, "variable name").lexeme;
        expect(TokenType::ASSIGN, "=");
        auto init = parseExpr();
        expect(TokenType::SEMICOLON, ";");
        return std::make_shared<DeclareStmt>(name, init);
    }
    if (check(TokenType::IDENTIFIER)) {
        std::string name = current.lexeme;
        advance();
        if (match(TokenType::ASSIGN)) {
            auto val = parseExpr();
            expect(TokenType::SEMICOLON, ";");
            return std::make_shared<AssignStmt>(name, val);
        }
        throw std::runtime_error("Unexpected token after identifier");
    }
    if (match(TokenType::RETURN)) {
        if (check(TokenType::SEMICOLON)) {
            advance();
            return std::make_shared<ReturnStmt>(nullptr);
        }
        auto val = parseExpr();
        expect(TokenType::SEMICOLON, ";");
        return std::make_shared<ReturnStmt>(val);
    }
    if (match(TokenType::IF)) {
        expect(TokenType::LPAREN, "(");
        auto cond = parseExpr();
        expect(TokenType::RPAREN, ")");
        auto thenStmt = parseStmt();
        std::shared_ptr<Stmt> elseStmt = nullptr;
        if (match(TokenType::ELSE)) {
            elseStmt = parseStmt();
        }
        return std::make_shared<IfStmt>(cond, thenStmt, elseStmt);
    }
    if (match(TokenType::WHILE)) {
        expect(TokenType::LPAREN, "(");
        auto cond = parseExpr();
        expect(TokenType::RPAREN, ")");
        auto body = parseStmt();
        return std::make_shared<WhileStmt>(cond, body);
    }
    if (match(TokenType::BREAK)) {
        expect(TokenType::SEMICOLON, ";");
        return std::make_shared<BreakStmt>();
    }
    if (match(TokenType::CONTINUE)) {
        expect(TokenType::SEMICOLON, ";");
        return std::make_shared<ContinueStmt>();
    }
    throw std::runtime_error("Unrecognized statement");
}

std::shared_ptr<Expr> Parser::parseExpr() {
    return parseLOrExpr();
}

std::shared_ptr<Expr> Parser::parseLOrExpr() {
    auto expr = parseLAndExpr();
    while (match(TokenType::OR)) {
        auto rhs = parseLAndExpr();
        expr = std::make_shared<BinaryExpr>("||", expr, rhs);
    }
    return expr;
}

std::shared_ptr<Expr> Parser::parseLAndExpr() {
    auto expr = parseRelExpr();
    while (match(TokenType::AND)) {
        auto rhs = parseRelExpr();
        expr = std::make_shared<BinaryExpr>("&&", expr, rhs);
    }
    return expr;
}

std::shared_ptr<Expr> Parser::parseRelExpr() {
    auto expr = parseAddExpr();
    while (check(TokenType::LT) || check(TokenType::GT) || check(TokenType::LE) ||
        check(TokenType::GE) || check(TokenType::EQ) || check(TokenType::NE)) {
        std::string op = current.lexeme;
        advance();
        auto rhs = parseAddExpr();
        expr = std::make_shared<BinaryExpr>(op, expr, rhs);
    }
    return expr;
}

std::shared_ptr<Expr> Parser::parseAddExpr() {
    auto expr = parseMulExpr();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        std::string op = current.lexeme;
        advance();
        auto rhs = parseMulExpr();
        expr = std::make_shared<BinaryExpr>(op, expr, rhs);
    }
    return expr;
}

std::shared_ptr<Expr> Parser::parseMulExpr() {
    auto expr = parseUnaryExpr();
    while (check(TokenType::MULT) || check(TokenType::DIV) || check(TokenType::MOD)) {
        std::string op = current.lexeme;
        advance();
        auto rhs = parseUnaryExpr();
        expr = std::make_shared<BinaryExpr>(op, expr, rhs);
    }
    return expr;
}

std::shared_ptr<Expr> Parser::parseUnaryExpr() {
    if (match(TokenType::PLUS)) {
        return parseUnaryExpr();
    }
    else if (match(TokenType::MINUS)) {
        auto zero = std::make_shared<NumberExpr>(0);
        auto expr = parseUnaryExpr();
        if (!expr) {
            throw std::runtime_error("Expected expression after '-'");
        }
        return std::make_shared<BinaryExpr>("-", zero, expr);
    }
    else if (match(TokenType::NOT)) {
        auto expr = parseUnaryExpr();
        if (!expr) {
            throw std::runtime_error("Expected expression after '!'");
        }
        return std::make_shared<BinaryExpr>("!", nullptr, expr);
    }
    else {
        return parsePrimaryExpr();
    }
}

std::shared_ptr<Expr> Parser::parsePrimaryExpr() {
    if (check(TokenType::NUMBER)) {
        std::string numberStr = current.lexeme;
        advance();
        int value = std::stoi(numberStr);
        return std::make_shared<NumberExpr>(value);
    }
    if (check(TokenType::IDENTIFIER)) {
        std::string name = current.lexeme;
        advance();
        if (check(TokenType::LPAREN)) {
            advance();
            std::vector<std::shared_ptr<Expr>> args;
            if (!check(TokenType::RPAREN)) {
                do {
                    args.push_back(parseExpr());
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::RPAREN, ")");
            auto call = std::make_shared<CallExpr>();
            call->callee = name;
            call->args = args;
            return call;
        }
        return std::make_shared<VariableExpr>(name);
    }
    if (match(TokenType::LPAREN)) {
        auto expr = parseExpr();
        expect(TokenType::RPAREN, ")");
        return expr;
    }
    throw std::runtime_error("Unexpected token in primary expression");
}