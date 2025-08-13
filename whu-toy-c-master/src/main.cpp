// src/main.cpp
#include "parser.tab.h"
#include "ast.h"
#include <iostream>
#include <algorithm>
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

    // 先保存原始指针，避免递归时丢失
    Expr* original = e;

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
                    case '+': result = make_int_const(val); break; // 处理 +123 情况
                }
                if (result) {
                    // 安全删除：先断开子节点连接
                    e->child = nullptr;
                    delete original;
                    return result;
                }
            }
            break;
            
        case Expr::BINARY_OP:
            e->left = optimize_expr(e->left);
            e->right = optimize_expr(e->right);
            // 检查左右子节点是否存在
            if (!e->left || !e->right) break;
            // 常量折叠：二元运算
            if (e->left && e->left->type == Expr::INT_CONST &&
                e->right && e->right->type == Expr::INT_CONST) {
                int l = e->left->int_val;
                int r = e->right->int_val;

                // 避免除零错误
                if ((e->op_str == "/" || e->op_str == "%") && r == 0) {
                    break; // 保留原表达式，不折叠
                }
                
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
                    e->left = nullptr;
                    e->right = nullptr;
                    delete original;
                    return result;
                }
            }
            break;
            
        case Expr::FUNC_CALL:
            for (auto& arg : e->args) 
                arg = optimize_expr(arg);
            break;
        case Expr::INT_CONST:
        case Expr::IDENTIFIER:
            // 叶子节点，无需优化
            break;
    }
    return e;
}

// 优化语句节点
static Stmt* optimize_stmt(Stmt* s) {
    if (!s) return nullptr;

    switch (s->type) {
        case Stmt::BLOCK:
            for (auto& stmt : s->block_stmts) 
                stmt = optimize_stmt(stmt);
            // 移除空语句块
            s->block_stmts.erase(
                std::remove_if(s->block_stmts.begin(), s->block_stmts.end(),
                    [](Stmt* stmt) { return stmt && stmt->type == Stmt::EMPTY; }),
                s->block_stmts.end());
            break;
            
        case Stmt::EXPR:
            if (s->expr_stmt) {
                s->expr_stmt = optimize_expr(s->expr_stmt);
            }
            break;
            
        case Stmt::ASSIGN:
            if (s->assign_rhs) {
                s->assign_rhs = optimize_expr(s->assign_rhs);
            }
            break;
            
        case Stmt::DECLARE:
            if (s->declare_init) {
                s->declare_init = optimize_expr(s->declare_init);
            }
            break;
            
        case Stmt::IF:
            if (s->if_cond) s->if_cond = optimize_expr(s->if_cond);
            if (s->if_then) s->if_then = optimize_stmt(s->if_then);
            if (s->if_else) s->if_else = optimize_stmt(s->if_else);
            
            // 死代码消除：条件恒为 false
            if (s->if_cond && s->if_cond->type == Expr::INT_CONST && s->if_cond->int_val == 0) {
                if (!s->if_else) {
                    // 只有if分支，整个if语句变为空
                    delete_expr(s->if_cond);
                    delete s->if_then;
                    s->type = Stmt::EMPTY;
                } else {
                    // 有else分支，替换为else语句
                    Stmt* else_stmt = s->if_else;
                    delete_expr(s->if_cond);
                    delete s->if_then;
                    s->if_else = nullptr;
                    delete s;
                    return else_stmt;
                }
            }
            // 条件恒为true，可以简化为只保留then分支
            else if (s->if_cond && s->if_cond->type == Expr::INT_CONST && s->if_cond->int_val != 0) {
                Stmt* then_stmt = s->if_then;
                delete_expr(s->if_cond);
                if (s->if_else) delete s->if_else;
                s->if_cond = nullptr;
                s->if_then = nullptr;
                s->if_else = nullptr;
                delete s;
                return then_stmt;
            }
            break;
        }
            
        case Stmt::WHILE: {
            if (s->while_cond) s->while_cond = optimize_expr(s->while_cond);
            if (s->while_body) s->while_body = optimize_stmt(s->while_body);
            
            // 死代码消除：条件恒为 false
            if (s->while_cond && s->while_cond->type == Expr::INT_CONST && s->while_cond->int_val == 0) {
                delete_expr(s->while_cond);
                delete s->while_body;
                s->type = Stmt::EMPTY;
            }
            break;
        }
            
        case Stmt::RETURN:
            if (s->return_expr) s->return_expr = optimize_expr(s->return_expr);
            break;
            
        default:
            // 其他语句类型不需要优化
            break;
    }
    return s;
}

// 优化整个编译单元
static void optimize_comp_unit(CompUnit* unit) {
   if (!unit) return;
    for (auto& func : unit->funcs) {
        if (func && func->body) {
            func->body = optimize_stmt(func->body);
            if (!func->body) {
                // 如果优化后body为空，创建一个空块
                func->body = make_block(std::vector<Stmt*>());
            }
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
