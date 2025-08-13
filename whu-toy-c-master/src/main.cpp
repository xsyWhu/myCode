// src/main.cpp
#include "parser.tab.h"
#include "ast.h"
#include <iostream>
#include "semantic.h"
#include "codegen.h"

//new adding
// 在 main.cpp 中添加以下函数

static Expr* optimize_expr(Expr* e);
static Stmt* optimize_stmt(Stmt* s);

// 优化表达式节点
static Expr* optimize_expr(Expr* e) {
    if (!e) return nullptr;

    switch (e->type) {
        case Expr::UNARY_OP:
            e->child = optimize_expr(e->child);
            if (e->child && e->child->type == Expr::INT_CONST) {
                int val = e->child->int_val;
                switch (e->op_char) {
                    case '-': return make_int_const(-val);
                    case '!': return make_int_const(!val);
                }
            }
            break;
        case Expr::BINARY_OP:
            e->left = optimize_expr(e->left);
            e->right = optimize_expr(e->right);
            if (e->left && e->left->type == Expr::INT_CONST &&
                e->right && e->right->type == Expr::INT_CONST) {
                int l = e->left->int_val;
                int r = e->right->int_val;
                if (e->op_str == "+") return make_int_const(l + r);
                if (e->op_str == "-") return make_int_const(l - r);
                if (e->op_str == "*") return make_int_const(l * r);
                if (e->op_str == "/" && r != 0) return make_int_const(l / r);
                if (e->op_str == "%" && r != 0) return make_int_const(l % r);
                if (e->op_str == "<") return make_int_const(l < r);
                if (e->op_str == ">") return make_int_const(l > r);
                if (e->op_str == "<=") return make_int_const(l <= r);
                if (e->op_str == ">=") return make_int_const(l >= r);
                if (e->op_str == "==") return make_int_const(l == r);
                if (e->op_str == "!=") return make_int_const(l != r);
                if (e->op_str == "&&") return make_int_const(l && r);
                if (e->op_str == "||") return make_int_const(l || r);
            }
            break;
        case Expr::FUNC_CALL:
            for (auto& arg : e->args) arg = optimize_expr(arg);
            break;
    }
    return e;
}

// 优化语句节点
static Stmt* optimize_stmt(Stmt* s) {
    if (!s) return nullptr;

    switch (s->type) {
        case Stmt::BLOCK:
            for (auto& stmt : s->block_stmts) stmt = optimize_stmt(stmt);
            break;
        case Stmt::EXPR:
            s->expr_stmt = optimize_expr(s->expr_stmt);
            break;
        case Stmt::ASSIGN:
            s->assign_rhs = optimize_expr(s->assign_rhs);
            break;
        case Stmt::DECLARE:
            s->declare_init = optimize_expr(s->declare_init);
            break;
        case Stmt::IF:
            s->if_cond = optimize_expr(s->if_cond);
            // 死代码消除：条件恒为 false
            if (s->if_cond->type == Expr::INT_CONST && s->if_cond->int_val == 0) {
                if (s->if_else) return optimize_stmt(s->if_else); // 只保留 else 分支
                return make_empty(); // 整个 if 可删除
            }
            s->if_then = optimize_stmt(s->if_then);
            //other
            if (s->if_else) s->if_else = optimize_stmt(s->if_else);
            break;
        case Stmt::WHILE:
            s->while_cond = optimize_expr(s->while_cond);
            // 死代码消除：条件恒为 false
            if (s->while_cond->type == Expr::INT_CONST && s->while_cond->int_val == 0) {
                return make_empty(); // 删除空循环
            }
            s->while_body = optimize_stmt(s->while_body);
            break;
        case Stmt::RETURN:
            if (s->return_expr) s->return_expr = optimize_expr(s->return_expr);
            break;
    }
    return s;
}

extern FILE* yyin; // flex 提供的输入文件指针

int main(int argc, char** argv) {
    // Always read from stdin (interactive or redirected)
    yyin = stdin;

    CompUnit* root = nullptr;
    int ret = yyparse(&root);
    if (ret != 0 || !root) {
        std::cerr << "Syntax analysis failed\n";
        return 1;
    }
    // std::cout << "Syntax analysis succeeded\n";

    // semantic
    std::vector<FuncInfo> funcs;
    if (!semantic_analyze(root, funcs)) {
        std::cerr << "Semantic analysis failed\n";
        delete root;
        return 1;
    }
    // std::cout << "Semantic analysis succeeded\n";

    //optimistic
    for(auto f : root -> funcs){
        f->body = optimize_stmt(f->body);
    }

    // generate assembly to stdout
    if (!generate_riscv(root, funcs, "-")) {
        std::cerr << "Code generation failed\n";
        delete root;
        return 1;
    }
    delete root;
    return 0;
}
