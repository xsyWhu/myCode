// src/codegen.cpp
#include "codegen.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <algorithm>

using namespace std;

/* label generator */
struct LabelGen {
    int id = 0;
    string next(const string &base) { return base + "_" + to_string(id++); }
} lab;

/* helper emit (to ostream) */
static void emit(ostream &o, const string &s) { o << s << "\n"; }

/* convert int offset to string "(offset)(s0)" format */
static string off_s0(int offset) {
    ostringstream ss;
    ss << offset << "(s0)";
    return ss.str();
}

static int align16(int x) { return ((x + 15) / 16) * 16; }

/* forward declarations */
static void gen_stmt(Stmt* s, const FuncInfo& fi, ostream &o, vector<pair<string,string>> &loop_stack, int &cur_sp_bytes);
static void gen_expr_stack(Expr* e, const FuncInfo& fi, ostream &o, int &cur_sp_bytes);
static void gen_expr_to_reg(Expr* e, const FuncInfo& fi, ostream &o, int &cur_sp_bytes, const string &reg);
static void emit_call(Expr* call_expr, const FuncInfo& fi, ostream &o, int &cur_sp_bytes, bool push_return);

/* push integer in t0 then onto stack and update cur_sp_bytes */
static void push_reg_t0(ostream &o, int &cur_sp_bytes) {
    emit(o, "addi sp, sp, -4");
    emit(o, "sw t0, 0(sp)");
    cur_sp_bytes += 4;
}

/* pop into t0 and update cur_sp_bytes */
static void pop_to_t0(ostream &o, int &cur_sp_bytes) {
    emit(o, "lw t0, 0(sp)");
    emit(o, "addi sp, sp, 4");
    cur_sp_bytes -= 4;
}

/* emit_call: evaluate args, align, push, load a0..a7, call; optionally push return */
static void emit_call(Expr* call_expr, const FuncInfo& fi, ostream &o, int &cur_sp_bytes, bool push_return) {
    size_t argc = call_expr->args.size();
    int total_args_bytes = (int)argc * 4;
    int pad = (16 - ((cur_sp_bytes + total_args_bytes) % 16)) % 16;
    if (pad > 0) {
        emit(o, "addi sp, sp, -" + to_string(pad));
        cur_sp_bytes += pad;
    }
    // evaluate arguments right-to-left and push
    for (int i = (int)argc - 1; i >= 0; --i) {
        gen_expr_stack(call_expr->args[i], fi, o, cur_sp_bytes);
    }
    // load into a0..a7 from stack (0(sp) is arg1)
    size_t in_regs = std::min<size_t>(8, argc);
    for (size_t i = 0; i < in_regs; ++i) {
        emit(o, "lw a" + to_string(i) + ", " + to_string((int)i*4) + "(sp)");
    }
    // call
    emit(o, "call " + call_expr->call_name);
    // restore caller stack (args + pad)
    if (total_args_bytes + pad > 0) {
        emit(o, "addi sp, sp, " + to_string(total_args_bytes + pad));
        cur_sp_bytes -= (total_args_bytes + pad);
    }
    if (push_return) {
        emit(o, "mv t0, a0");
        push_reg_t0(o, cur_sp_bytes);
    } else {
        // return in a0 already
    }
}

/* gen_expr_stack: stack-based evaluator (result pushed) */
static void gen_expr_stack(Expr* e, const FuncInfo& fi, ostream &o, int &cur_sp_bytes) {
    if (!e) return;
    switch (e->type) {
        case Expr::INT_CONST: {
            emit(o, "li t0, " + to_string(e->int_val));
            push_reg_t0(o, cur_sp_bytes);
            break;
        }
        case Expr::IDENTIFIER: {
            auto it = fi.expr_resolved_offset.find(e);
            assert(it != fi.expr_resolved_offset.end());
            int off = it->second;
            emit(o, "lw t0, " + off_s0(off));
            push_reg_t0(o, cur_sp_bytes);
            break;
        }
        case Expr::UNARY_OP: {
            gen_expr_stack(e->child, fi, o, cur_sp_bytes);
            pop_to_t0(o, cur_sp_bytes);
            if (e->op_char == '-') {
                emit(o, "sub t0, zero, t0");
            } else if (e->op_char == '!') {
                emit(o, "sltu t0, zero, t0");
                emit(o, "xori t0, t0, 1");
            }
            push_reg_t0(o, cur_sp_bytes);
            break;
        }
        case Expr::BINARY_OP: {
            if (e->op_str == "&&") {
                string Lfalse = lab.next("Lfalse");
                string Lend = lab.next("Lend");
                gen_expr_stack(e->left, fi, o, cur_sp_bytes);
                pop_to_t0(o, cur_sp_bytes);
                emit(o, "beqz t0, " + Lfalse);
                gen_expr_stack(e->right, fi, o, cur_sp_bytes);
                pop_to_t0(o, cur_sp_bytes);
                emit(o, "sltu t0, zero, t0");
                emit(o, "j " + Lend);
                emit(o, Lfalse + ":");
                emit(o, "li t0, 0");
                emit(o, Lend + ":");
                push_reg_t0(o, cur_sp_bytes);
                return;
            } else if (e->op_str == "||") {
                string Ltrue = lab.next("Ltrue");
                string Lend = lab.next("Lend");
                gen_expr_stack(e->left, fi, o, cur_sp_bytes);
                pop_to_t0(o, cur_sp_bytes);
                emit(o, "bnez t0, " + Ltrue);
                gen_expr_stack(e->right, fi, o, cur_sp_bytes);
                pop_to_t0(o, cur_sp_bytes);
                emit(o, "sltu t0, zero, t0");
                emit(o, "j " + Lend);
                emit(o, Ltrue + ":");
                emit(o, "li t0, 1");
                emit(o, Lend + ":");
                push_reg_t0(o, cur_sp_bytes);
                return;
            }

            gen_expr_stack(e->left, fi, o, cur_sp_bytes);
            gen_expr_stack(e->right, fi, o, cur_sp_bytes);
            pop_to_t0(o, cur_sp_bytes); // right -> t0
            emit(o, "mv t1, t0"); // t1 = right
            pop_to_t0(o, cur_sp_bytes); // left -> t0
            const string &op = e->op_str;
            if (op == "+") emit(o, "add t0, t0, t1");
            else if (op == "-") emit(o, "sub t0, t0, t1");
            else if (op == "*") emit(o, "mul t0, t0, t1");
            else if (op == "/") emit(o, "div t0, t0, t1");
            else if (op == "%") emit(o, "rem t0, t0, t1");
            else if (op == "<") emit(o, "slt t0, t0, t1");
            else if (op == ">") emit(o, "slt t0, t1, t0");
            else if (op == "<=") { emit(o, "slt t2, t1, t0"); emit(o, "xori t0, t2, 1"); }
            else if (op == ">=") { emit(o, "slt t2, t0, t1"); emit(o, "xori t0, t2, 1"); }
            else if (op == "==") {
                emit(o, "xor t2, t0, t1");
                emit(o, "sltu t0, zero, t2");
                emit(o, "xori t0, t0, 1");
            } else if (op == "!=") {
                emit(o, "xor t2, t0, t1");
                emit(o, "sltu t0, zero, t2");
            } else {
                emit(o, "# unknown op: " + op);
            }
            push_reg_t0(o, cur_sp_bytes);
            break;
        }
        case Expr::FUNC_CALL: {
            emit_call(e, fi, o, cur_sp_bytes, true);
            break;
        }
    }
}

/* gen_expr_to_reg: try to put result directly into reg (e.g., a0) */
static void gen_expr_to_reg(Expr* e, const FuncInfo& fi, ostream &o, int &cur_sp_bytes, const string &reg) {
    if (!e) return;
    switch (e->type) {
        case Expr::INT_CONST:
            emit(o, "li " + reg + ", " + to_string(e->int_val));
            return;
        case Expr::IDENTIFIER: {
            auto it = fi.expr_resolved_offset.find(e);
            assert(it != fi.expr_resolved_offset.end());
            int off = it->second;
            emit(o, "lw " + reg + ", " + off_s0(off));
            return;
        }
        case Expr::FUNC_CALL:
            emit_call(e, fi, o, cur_sp_bytes, false); // leave result in a0
            if (reg != "a0") emit(o, "mv " + reg + ", a0");
            return;
        default:
            gen_expr_stack(e, fi, o, cur_sp_bytes);
            pop_to_t0(o, cur_sp_bytes);
            emit(o, "mv " + reg + ", t0");
            return;
    }
}

/* generate statement */
static void gen_stmt(Stmt* s, const FuncInfo& fi, ostream &o, vector<pair<string,string>> &loop_stack, int &cur_sp_bytes) {
    if (!s) return;
    switch (s->type) {
        case Stmt::BLOCK:
            for (auto sub : s->block_stmts) gen_stmt(sub, fi, o, loop_stack, cur_sp_bytes);
            break;
        case Stmt::EMPTY:
            break;
        case Stmt::EXPR:
            gen_expr_stack(s->expr_stmt, fi, o, cur_sp_bytes);
            pop_to_t0(o, cur_sp_bytes);
            break;
        case Stmt::DECLARE: {
            gen_expr_stack(s->declare_init, fi, o, cur_sp_bytes);
            pop_to_t0(o, cur_sp_bytes);
            int off = fi.stmt_lhs_offset.at(s);
            emit(o, "sw t0, " + off_s0(off));
            break;
        }
        case Stmt::ASSIGN: {
            gen_expr_stack(s->assign_rhs, fi, o, cur_sp_bytes);
            pop_to_t0(o, cur_sp_bytes);
            int off = fi.stmt_lhs_offset.at(s);
            emit(o, "sw t0, " + off_s0(off));
            break;
        }
        case Stmt::IF: {
            string Lelse = lab.next("Lelse");
            string Lend = lab.next("Lend");
            gen_expr_stack(s->if_cond, fi, o, cur_sp_bytes);
            pop_to_t0(o, cur_sp_bytes);
            emit(o, "beqz t0, " + Lelse);
            gen_stmt(s->if_then, fi, o, loop_stack, cur_sp_bytes);
            emit(o, "j " + Lend);
            emit(o, Lelse + ":");
            if (s->if_else) gen_stmt(s->if_else, fi, o, loop_stack, cur_sp_bytes);
            emit(o, Lend + ":");
            break;
        }
        case Stmt::WHILE: {
            string Lbegin = lab.next("Lwhile_begin");
            string Lend = lab.next("Lwhile_end");
            emit(o, Lbegin + ":");
            gen_expr_stack(s->while_cond, fi, o, cur_sp_bytes);
            pop_to_t0(o, cur_sp_bytes);
            emit(o, "beqz t0, " + Lend);
            loop_stack.emplace_back(Lbegin, Lend);
            gen_stmt(s->while_body, fi, o, loop_stack, cur_sp_bytes);
            loop_stack.pop_back();
            emit(o, "j " + Lbegin);
            emit(o, Lend + ":");
            break;
        }
        case Stmt::BREAK:
            if (loop_stack.empty()) emit(o, "# break used outside loop");
            else emit(o, "j " + loop_stack.back().second);
            break;
        case Stmt::CONTINUE:
            if (loop_stack.empty()) emit(o, "# continue used outside loop");
            else emit(o, "j " + loop_stack.back().first);
            break;
        case Stmt::RETURN:
            if (s->return_expr) {
                gen_expr_to_reg(s->return_expr, fi, o, cur_sp_bytes, "a0");
            }
            emit(o, "j __func_end_" + fi.name);
            break;
        default:
            emit(o, "# unknown stmt");
    }
}

bool generate_riscv(CompUnit* root, const vector<FuncInfo>& funcs, const string& out_path) {
    ostream* out_stream;
    ofstream ofs;
    if (out_path == "-") {
        out_stream = &cout;
    } else {
        ofs.open(out_path);
        if (!ofs.is_open()) {
            cerr << "failed to open output: " << out_path << "\n";
            return false;
        }
        out_stream = &ofs;
    }
    ostream &o = *out_stream;

    // emit(o, ".file \"" + out_path + "\"");
    // emit(o, ".text");

    for (size_t i = 0; i < root->funcs.size(); ++i) {
        FuncDef* f = root->funcs[i];
        const FuncInfo& fi = funcs[i];

        int total_slots = (int)fi.params.size() + fi.num_locals;
        int frame_size = align16(12 + 4 * total_slots);

        emit(o, ".globl " + fi.name);
        emit(o, fi.name + ":");
        // prologue
        emit(o, "addi sp, sp, -" + to_string(frame_size));
        emit(o, "sw ra, " + to_string(frame_size - 4) + "(sp)");
        emit(o, "sw s0, " + to_string(frame_size - 8) + "(sp)");
        emit(o, "addi s0, sp, " + to_string(frame_size));

        // store a0..a7 into local slots for params 0..7
        for (size_t pi = 0; pi < fi.params.size(); ++pi) {
            int off = fi.var_offset.at(fi.params[pi]); // this is the assigned local slot (negative)
            if (pi < 8) {
                emit(o, "sw a" + to_string(pi) + ", " + off_s0(off));
            } else {
                // param was passed on caller stack at s0 + (pi*4)
                emit(o, "lw t0, " + to_string((int)pi*4) + "(s0)");
                emit(o, "sw t0, " + off_s0(off)); // copy into local slot
            }
        }

        // body
        vector<pair<string,string>> loop_stack;
        int cur_sp_bytes = 0; // bytes pushed since prologue
        gen_stmt(f->body, fi, o, loop_stack, cur_sp_bytes);

        // epilogue
        emit(o, "__func_end_" + fi.name + ":");
        emit(o, "lw ra, " + to_string(frame_size - 4) + "(sp)");
        emit(o, "lw s0, " + to_string(frame_size - 8) + "(sp)");
        emit(o, "addi sp, sp, " + to_string(frame_size));
        emit(o, "jr ra");
        emit(o, "");
    }

    if (ofs.is_open()) ofs.close();
    return true;
}
