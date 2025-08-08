#pragma once
#include <string>
#include <memory>
#include <vector>

// ���� AST ����
struct ASTNode {
    virtual ~ASTNode() = default;
};

struct Expr : ASTNode {};
struct Stmt : ASTNode {};

struct Param {
    std::string name;
};

// ����ʽ��
struct NumberExpr : Expr {
    int value;
    explicit NumberExpr(int val) : value(val) {}
};

struct VariableExpr : Expr {
    std::string name;
    explicit VariableExpr(const std::string& n) : name(n) {}
};

struct BinaryExpr : Expr {
    std::string op;
    std::shared_ptr<Expr> lhs, rhs;
    BinaryExpr(const std::string& o, std::shared_ptr<Expr> l, std::shared_ptr<Expr> r)
        : op(o), lhs(l), rhs(r) {}
};

struct CallExpr : Expr {
    std::string callee;
    std::vector<std::shared_ptr<Expr>> args;
};

// �����
struct ExprStmt : Stmt {
    std::shared_ptr<Expr> expr;
    ExprStmt(std::shared_ptr<Expr> e) : expr(std::move(e)) {}
};

struct ReturnStmt : Stmt {
    std::shared_ptr<Expr> value;
    ReturnStmt(std::shared_ptr<Expr> v) : value(std::move(v)) {}
};

struct BlockStmt : Stmt {
    std::vector<std::shared_ptr<Stmt>> statements;
    // 添加构造函数
    BlockStmt() = default;
    explicit BlockStmt(std::vector<std::shared_ptr<Stmt>> stmts) : statements(std::move(stmts)) {}
};

struct IfStmt : Stmt {
    std::shared_ptr<Expr> condition;
    std::shared_ptr<Stmt> thenStmt;
    std::shared_ptr<Stmt> elseStmt;
    IfStmt(std::shared_ptr<Expr> cond, std::shared_ptr<Stmt> thenS, std::shared_ptr<Stmt> elseS = nullptr)
        : condition(std::move(cond)), thenStmt(std::move(thenS)), elseStmt(std::move(elseS)) {}
};

struct WhileStmt : Stmt {
    std::shared_ptr<Expr> condition;
    std::shared_ptr<Stmt> body;
    WhileStmt(std::shared_ptr<Expr> cond, std::shared_ptr<Stmt> b)
        : condition(std::move(cond)), body(std::move(b)) {}
};

struct AssignStmt : Stmt {
    std::string varName;
    std::shared_ptr<Expr> value;
    AssignStmt(std::string name, std::shared_ptr<Expr> val)
        : varName(std::move(name)), value(std::move(val)) {}
};

struct DeclareStmt : Stmt {
    std::string varName;
    std::shared_ptr<Expr> initVal;
    DeclareStmt(std::string name, std::shared_ptr<Expr> init)
        : varName(std::move(name)), initVal(std::move(init)) {}
};

struct FuncDef : ASTNode {
    std::string retType;
    std::string name;
    std::vector<Param> params;
    std::shared_ptr<BlockStmt> body;
};

class BreakStmt : public Stmt {
public:
    BreakStmt() = default;
};

class ContinueStmt : public Stmt {
public:
    ContinueStmt() = default;
};
