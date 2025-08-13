// src/main.cpp
#include "parser.tab.h"
#include "ast.h"
#include <iostream>
#include <algorithm>
#include "semantic.h"
#include "codegen.h"

/* ========= 安全优化补丁开始 ========= */

// 安全的删除表达式：先把子树全部置空再 delete
static void safe_delete_expr(Expr* e) {
    if (!e) return;
    switch (e->type) {
        case Expr::UNARY_OP:  e->child = nullptr; break;
        case Expr::BINARY_OP: e->left = e->right = nullptr; break;
        case Expr::FUNC_CALL: e->args.clear(); break;
        default: break;
    }
    delete e;
}

// 安全删除语句
static void safe_delete_stmt(Stmt* s) {
    if (!s) return;
    s->block_stmts.clear();
    delete s;
}

// 常量折叠 & 死代码消除
static Expr* fold(Expr* e) {
    if (!e) return e;

    switch (e->type) {
        case Expr::UNARY_OP:
            e->child = fold(e->child);
            if (e->child && e->child->type == Expr::INT_CONST) {
                int v = e->child->int_val;
                Expr* res = nullptr;
                switch (e->op_char) {
                    case '+': res = make_int_const(v); break;
                    case '-': res = make_int_const(-v); break;
                    case '!': res = make_int_const(!v); break;
                }
                if (res) { safe_delete_expr(e); return res; }
            }
            break;

        case Expr::BINARY_OP:
            e->left  = fold(e->left);
            e->right = fold(e->right);
            if (e->left  && e->left->type  == Expr::INT_CONST &&
                e->right && e->right->type == Expr::INT_CONST) {
                int l = e->left->int_val, r = e->right->int_val;
                Expr* res = nullptr;
                if      (e->op_str == "+") res = make_int_const(l + r);
                else if (e->op_str == "-") res = make_int_const(l - r);
                else if (e->op_str == "*") res = make_int_const(l * r);
                else if (e->op_str == "/" && r != 0) res = make_int_const(l / r);
                else if (e->op_str == "%" && r != 0) res = make_int_const(l % r);
                else if (e->op_str == "<")  res = make_int_const(l < r);
                else if (e->op_str == ">")  res = make_int_const(l > r);
                else if (e->op_str == "<=") res = make_int_const(l <= r);
                else if (e->op_str == ">=") res = make_int_const(l >= r);
                else if (e->op_str == "==") res = make_int_const(l == r);
                else if (e->op_str == "!=") res = make_int_const(l != r);
                else if (e->op_str == "&&") res = make_int_const(l && r);
                else if (e->op_str == "||") res = make_int_const(l || r);
                if (res) { safe_delete_expr(e); return res; }
            }
            break;

        case Expr::FUNC_CALL:
            for (auto& arg : e->args) arg = fold(arg);
            break;

        default: break;
    }
    return e;
}

static Stmt* opt(Stmt* s) {
    if (!s) return s;

    switch (s->type) {
        case Stmt::BLOCK: {
            for (auto& st : s->block_stmts) st = opt(st);
            s->block_stmts.erase(
                std::remove_if(s->block_stmts.begin(), s->block_stmts.end(),
                               [](Stmt* st){ return !st || st->type == Stmt::EMPTY; }),
                s->block_stmts.end());
            break;
        }
        case Stmt::EXPR:      s->expr_stmt   = fold(s->expr_stmt); break;
        case Stmt::ASSIGN:    s->assign_rhs  = fold(s->assign_rhs); break;
        case Stmt::DECLARE:   s->declare_init= fold(s->declare_init); break;
        case Stmt::RETURN:    if (s->return_expr) s->return_expr = fold(s->return_expr); break;
        case Stmt::IF:
            s->if_cond = fold(s->if_cond);
            s->if_then = opt(s->if_then);
            if (s->if_else) s->if_else = opt(s->if_else);
            if (s->if_cond && s->if_cond->type == Expr::INT_CONST) {
                if (s->if_cond->int_val == 0) {
                    safe_delete_expr(s->if_cond);
                    safe_delete_stmt(s->if_then);
                    if (!s->if_else) { safe_delete_stmt(s); return make_empty(); }
                    Stmt* else_stmt = s->if_else;
                    safe_delete_stmt(s);
                    return else_stmt;
                } else {
                    safe_delete_expr(s->if_cond);
                    if (s->if_else) safe_delete_stmt(s->if_else);
                    Stmt* then_stmt = s->if_then;
                    safe_delete_stmt(s);
                    return then_stmt;
                }
            }
            break;
        case Stmt::WHILE:
            s->while_cond = fold(s->while_cond);
            s->while_body = opt(s->while_body);
            if (s->while_cond && s->while_cond->type == Expr::INT_CONST && s->while_cond->int_val == 0) {
                safe_delete_expr(s->while_cond);
                safe_delete_stmt(s->while_body);
                safe_delete_stmt(s);
                return make_empty();
            }
            break;
        default: break;
    }
    return s;
}

static void optimize_all(CompUnit* root) {
    if (!root) return;
    for (auto f : root->funcs) if (f && f->body) f->body = opt(f->body);
}
/* ========= 安全优化补丁结束 ========= */

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

    optimize_all(root);

    // generate assembly to stdout
    if (!generate_riscv(root, funcs, "-")) {
        std::cerr << "Code generation failed\n";
        delete root;
        return 1;
    }
    delete root;
    return 0;
}
