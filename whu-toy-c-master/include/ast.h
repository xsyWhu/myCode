#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <iostream>

/* 前向声明 */
struct Expr;
struct Stmt;
struct FuncDef;
struct CompUnit;

/* 表达式 */
struct Expr {
    enum Type {
        INT_CONST,
        IDENTIFIER,
        UNARY_OP,
        BINARY_OP,
        FUNC_CALL
    } type;

    int int_val = 0;                 // INT_CONST
    std::string id_name;             // IDENTIFIER
    Expr* child = nullptr;           // UNARY_OP -> child
    char op_char = 0;                // UNARY_OP op
    Expr* left = nullptr;            // BINARY_OP left
    Expr* right = nullptr;           // BINARY_OP right
    std::string op_str;              // BINARY_OP op
    std::string call_name;           // FUNC_CALL name
    std::vector<Expr*> args;         // FUNC_CALL args

    Expr() = default;

    ~Expr() {
        delete child;
        delete left;
        delete right;
        for (Expr* e : args) delete e;
    }
};

/* 语句 */
struct Stmt {
    enum Type {
        BLOCK,
        EMPTY,
        EXPR,
        ASSIGN,
        DECLARE,
        IF,
        WHILE,
        BREAK,
        CONTINUE,
        RETURN
    } type;

    std::vector<Stmt*> block_stmts;  // BLOCK
    Expr* expr_stmt = nullptr;       // EXPR
    std::string assign_id;           // ASSIGN id
    Expr* assign_rhs = nullptr;      // ASSIGN rhs
    std::string declare_id;          // DECLARE id
    Expr* declare_init = nullptr;    // DECLARE init
    Expr* if_cond = nullptr;         // IF cond
    Stmt* if_then = nullptr;         // IF then
    Stmt* if_else = nullptr;         // IF else
    Expr* while_cond = nullptr;      // WHILE cond
    Stmt* while_body = nullptr;      // WHILE body
    Expr* return_expr = nullptr;     // RETURN expr

    Stmt() = default;

    ~Stmt() {
        for (Stmt* s : block_stmts) delete s;
        delete expr_stmt;
        delete assign_rhs;
        delete declare_init;
        delete if_cond;
        delete if_then;
        delete if_else;
        delete while_cond;
        delete while_body;
        delete return_expr;
    }
};

/* 函数定义 */
struct FuncDef {
    std::string return_type;
    std::string name;
    std::vector<std::string> params;
    Stmt* body = nullptr;

    ~FuncDef() {
        delete body;
    }
};

/* 编译单元 */
struct CompUnit {
    std::vector<FuncDef*> funcs;
    ~CompUnit() {
        for (FuncDef* f : funcs) delete f;
    }
};

/* ===== AST 打印工具（便于调试和验证） ===== */
inline void print_indent(int n) {
    for (int i = 0; i < n; ++i) std::cout << "  ";
}

inline void print_expr(const Expr* e, int indent = 0) {
    if (!e) { print_indent(indent); std::cout << "NULL\n"; return; }
    print_indent(indent);
    switch (e->type) {
        case Expr::INT_CONST:
            std::cout << "IntConst: " << e->int_val << "\n";
            break;
        case Expr::IDENTIFIER:
            std::cout << "Identifier: " << e->id_name << "\n";
            break;
        case Expr::UNARY_OP:
            std::cout << "UnaryOp: '" << e->op_char << "'\n";
            print_expr(e->child, indent + 1);
            break;
        case Expr::BINARY_OP:
            std::cout << "BinaryOp: " << e->op_str << "\n";
            print_expr(e->left, indent + 1);
            print_expr(e->right, indent + 1);
            break;
        case Expr::FUNC_CALL:
            std::cout << "FuncCall: " << e->call_name << "\n";
            for (auto arg : e->args) print_expr(arg, indent + 1);
            break;
    }
}

inline void print_stmt(const Stmt* s, int indent = 0) {
    if (!s) { print_indent(indent); std::cout << "NULL\n"; return; }
    print_indent(indent);
    switch (s->type) {
        case Stmt::BLOCK:
            std::cout << "Block {\n";
            for (auto sub : s->block_stmts) print_stmt(sub, indent + 1);
            print_indent(indent);
            std::cout << "}\n";
            break;
        case Stmt::EMPTY:
            std::cout << "EmptyStmt\n";
            break;
        case Stmt::EXPR:
            std::cout << "ExprStmt\n";
            print_expr(s->expr_stmt, indent + 1);
            break;
        case Stmt::ASSIGN:
            std::cout << "Assign: " << s->assign_id << "\n";
            print_expr(s->assign_rhs, indent + 1);
            break;
        case Stmt::DECLARE:
            std::cout << "Declare: " << s->declare_id << "\n";
            print_expr(s->declare_init, indent + 1);
            break;
        case Stmt::IF:
            std::cout << "If\n";
            print_indent(indent + 1); std::cout << "Cond:\n";
            print_expr(s->if_cond, indent + 2);
            print_indent(indent + 1); std::cout << "Then:\n";
            print_stmt(s->if_then, indent + 2);
            if (s->if_else) {
                print_indent(indent + 1); std::cout << "Else:\n";
                print_stmt(s->if_else, indent + 2);
            }
            break;
        case Stmt::WHILE:
            std::cout << "While\n";
            print_indent(indent + 1); std::cout << "Cond:\n";
            print_expr(s->while_cond, indent + 2);
            print_indent(indent + 1); std::cout << "Body:\n";
            print_stmt(s->while_body, indent + 2);
            break;
        case Stmt::BREAK:
            std::cout << "Break\n"; break;
        case Stmt::CONTINUE:
            std::cout << "Continue\n"; break;
        case Stmt::RETURN:
            std::cout << "Return\n";
            if (s->return_expr) print_expr(s->return_expr, indent + 1);
            break;
        default:
            std::cout << "UnknownStmt\n";
    }
}

inline void print_ast(const CompUnit* root, int indent = 0) {
    if (!root) { std::cout << "Empty AST\n"; return; }
    std::cout << "CompUnit {\n";
    for (auto f : root->funcs) {
        print_indent(indent + 1);
        std::cout << "FuncDef: " << f->name << " return " << f->return_type << "\n";
        print_indent(indent + 2); std::cout << "Params: ";
        for (auto &p : f->params) std::cout << p << " ";
        std::cout << "\n";
        print_indent(indent + 2); std::cout << "Body:\n";
        print_stmt(f->body, indent + 3);
    }
    std::cout << "}\n";
}

#endif // AST_H
