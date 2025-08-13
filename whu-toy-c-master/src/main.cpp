// src/main.cpp
#include "parser.tab.h"
#include "ast.h"
#include <iostream>
#include "semantic.h"
#include "codegen.h"

// 在 main.cpp 中添加以下代码

// 创建整数常量表达式
static Expr* make_int_const(int value) {
    Expr* e = new Expr();
    e->type = Expr::INT_CONST;
    e->int_val = value;
    return e;
}

// 递归删除表达式节点
static void delete_expr(Expr* e) {
    if (!e) return;
    switch (e->type) {
        case Expr::UNARY_OP:
            delete_expr(e->child);
            break;
        case Expr::BINARY_OP:
            delete_expr(e->left);
            delete_expr(e->right);
            break;
        case Expr::FUNC_CALL:
            for (auto arg : e->args) delete_expr(arg);
            break;
    }
    delete e;
}

// 优化表达式节点
static Expr* optimize_expr(Expr* e) {
    if (!e) return nullptr;

    // 递归优化子表达式
    switch (e->type) {
        case Expr::UNARY_OP:
            e->child = optimize_expr(e->child);
            // 常量折叠：一元运算
            if (e->child && e->child->type == Expr::INT_CONST) {
                int val = e->child->int_val;
                Expr* result = nullptr;
                switch (e->op_char) {
                    case '-': result = make_int_const(-val); break;
                    case '!': result = make_int_const(!val); break;
                }
                if (result) {
                    delete_expr(e); // 删除原节点
                    return result;
                }
            }
            break;
            
        case Expr::BINARY_OP:
            e->left = optimize_expr(e->left);
            e->right = optimize_expr(e->right);
            // 常量折叠：二元运算
            if (e->left && e->left->type == Expr::INT_CONST &&
                e->right && e->right->type == Expr::INT_CONST) {
                int l = e->left->int_val;
                int r = e->right->int_val;
                Expr* result = nullptr;
                
                if (e->op_str == "+") result = make_int_const(l + r);
                else if (e->op_str == "-") result = make_int_const(l - r);
                else if (e->op_str == "*") result = make_int_const(l * r);
                else if (e->op_str == "/" && r != 0) result = make_int_const(l / r);
                else if (e->op_str == "%" && r != 0) result = make_int_const(l % r);
                else if (e->op_str == "<") result = make_int_const(l < r);
                else if (e->op_str == ">") result = make_int_const(l > r);
                else if (e->op_str == "<=") result = make_int_const(l <= r);
                else if (e->op_str == ">=") result = make_int_const(l >= r);
                else if (e->op_str == "==") result = make_int_const(l == r);
                else if (e->op_str == "!=") result = make_int_const(l != r);
                else if (e->op_str == "&&") result = make_int_const(l && r);
                else if (e->op_str == "||") result = make_int_const(l || r);
                
                if (result) {
                    delete_expr(e); // 删除原节点
                    return result;
                }
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
            // 移除空语句块
            s->block_stmts.erase(
                std::remove_if(s->block_stmts.begin(), s->block_stmts.end(),
                    [](Stmt* stmt) { return stmt && stmt->type == Stmt::EMPTY; }),
                s->block_stmts.end());
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
            s->if_then = optimize_stmt(s->if_then);
            if (s->if_else) s->if_else = optimize_stmt(s->if_else);
            
            // 死代码消除：条件恒为 false
            if (s->if_cond && s->if_cond->type == Expr::INT_CONST && s->if_cond->int_val == 0) {
                // 删除整个 if 块
                if (!s->if_else) {
                    delete_expr(s->if_cond);
                    delete s->if_then;
                    s->type = Stmt::EMPTY;
                } 
                // 只保留 else 分支
                else {
                    Stmt* else_stmt = s->if_else;
                    delete_expr(s->if_cond);
                    delete s->if_then;
                    delete s; // 删除当前 if 节点
                    return optimize_stmt(else_stmt);
                }
            }
            break;
            
        case Stmt::WHILE:
            s->while_cond = optimize_expr(s->while_cond);
            s->while_body = optimize_stmt(s->while_body);
            
            // 死代码消除：条件恒为 false
            if (s->while_cond && s->while_cond->type == Expr::INT_CONST && s->while_cond->int_val == 0) {
                delete_expr(s->while_cond);
                delete s->while_body;
                s->type = Stmt::EMPTY;
            }
            break;
            
        case Stmt::RETURN:
            if (s->return_expr) s->return_expr = optimize_expr(s->return_expr);
            break;
    }
    return s;
}

// 优化整个编译单元
static void optimize_comp_unit(CompUnit* unit) {
    for (auto func : unit->funcs) {
        if (func->body) {
            func->body = optimize_stmt(func->body);
        }
    }
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

    // 优化阶段：常量折叠 + 死代码消除
    optimize_comp_unit(root);

    // generate assembly to stdout
    if (!generate_riscv(root, funcs, "-")) {
        std::cerr << "Code generation failed\n";
        delete root;
        return 1;
    }
    delete root;
    return 0;
}
