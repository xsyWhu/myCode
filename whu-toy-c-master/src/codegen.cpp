// src/codegen.cpp  (最终零嵌套版：CSE + Peephole)
#include "codegen.h"
#include <sstream>
#include <unordered_map>
#include <vector>
#include <fstream>      
#include <algorithm>

using namespace std;

/* ---------- 窥孔缓存 ---------- */
static vector<string> peephole_buf;
static void flush_peephole(std::ostream& o) {
    for (const string& s : peephole_buf) o << s << '\n';
    peephole_buf.clear();
}
static void emit(std::ostream& o, const string& s) {
    peephole_buf.push_back(s);
    if (peephole_buf.size() < 3) return;
    string& a = peephole_buf[peephole_buf.size() - 2];
    string& b = peephole_buf[peephole_buf.size() - 1];
    if ((a.find("addi sp, sp, -") != string::npos || a.find("addi sp,sp,-") != string::npos) &&
        (b.find("addi sp, sp, ") != string::npos && b.find("addi sp,sp,") != string::npos)) {
        int v1 = stoi(a.substr(a.find_last_of('-') != string::npos ?
                               a.find_last_of('-') : a.find_last_of(' ')));
        int v2 = stoi(b.substr(b.find_last_of(' ')));
        if (v1 == v2) { peephole_buf.pop_back(); peephole_buf.pop_back(); return; }
    }
    o << peephole_buf[peephole_buf.size() - 3] << '\n';
}

/* ---------- CSE ---------- */
static unordered_map<string, int> cse_map;
static string expr_key(const string& op, int loff, int roff) {
    return op + "_" + to_string(loff) + "_" + to_string(roff);
}

/* ---------- 小工具 ---------- */
struct LabelGen { int id = 0; string next(const string& b) { return b + "_" + to_string(id++); } } lab;
static int align16(int x) { return (x + 15) & ~15; }
static string off_s0(int o) { char buf[32]; sprintf(buf, "%d(s0)", o); return buf; }
static void push_reg_t0(ostream& o, int& sp) { emit(o, "addi sp, sp, -4"); emit(o, "sw t0, 0(sp)"); sp += 4; }
static void pop_to_t0(ostream& o, int& sp) { emit(o, "lw t0, 0(sp)"); emit(o, "addi sp, sp, 4"); sp -= 4; }

/* ---------- 前向声明 ---------- */
static void gen_expr_stack(Expr*, const FuncInfo&, ostream&, int&);
static void gen_expr_to_reg(Expr*, const FuncInfo&, ostream&, int&, const string&);
static void gen_stmt(Stmt*, const FuncInfo&, ostream&, vector<pair<string, string>>&, int&);

/* ---------- 表达式生成 ---------- */
static void gen_expr_to_reg(Expr* e, const FuncInfo& fi, ostream& o, int& sp, const string& reg) {
    if (!e) { emit(o, "li " + reg + ", 0"); return; }
    switch (e->type) {
        case Expr::INT_CONST:
            emit(o, "li " + reg + ", " + to_string(e->int_val)); break;
        case Expr::IDENTIFIER: {
            int off = fi.expr_resolved_offset.at(e);
            emit(o, "lw " + reg + ", " + off_s0(off)); break;
        }
        case Expr::FUNC_CALL: {
            size_t argc = e->args.size();
            int total = argc * 4, pad = (16 - ((sp + total) & 15)) & 15;
            if (pad) { emit(o, "addi sp, sp, -" + to_string(pad)); sp += pad; }
            for (int i = argc - 1; i >= 0; --i) gen_expr_stack(e->args[i], fi, o, sp);
            for (size_t i = 0; i < min(argc, (size_t)8); ++i)
                emit(o, "lw a" + to_string(i) + ", " + to_string(i * 4) + "(sp)");
            emit(o, "call " + e->call_name);
            if (total + pad) { emit(o, "addi sp, sp, " + to_string(total + pad)); sp -= (total + pad); }
            if (reg != "a0") emit(o, "mv " + reg + ", a0");
            break;
        }
        default:
            gen_expr_stack(e, fi, o, sp); pop_to_t0(o, sp);
            emit(o, "mv " + reg + ", t0"); break;
    }
}

static void gen_expr_stack(Expr* e, const FuncInfo& fi, ostream& o, int& sp) {
    if (!e) return;
    switch (e->type) {
        case Expr::INT_CONST:
            emit(o, "li t0, " + to_string(e->int_val)); push_reg_t0(o, sp); break;
        case Expr::IDENTIFIER: {
            int off = fi.expr_resolved_offset.at(e);
            emit(o, "lw t0, " + off_s0(off)); push_reg_t0(o, sp); break;
        }
        case Expr::UNARY_OP:
            gen_expr_stack(e->child, fi, o, sp); pop_to_t0(o, sp);
            if (e->op_char == '-') emit(o, "sub t0, zero, t0");
            else if (e->op_char == '!') { emit(o, "sltu t0, zero, t0"); emit(o, "xori t0, t0, 1"); }
            push_reg_t0(o, sp); break;
        case Expr::BINARY_OP:
            if (e->op_str == "&&" || e->op_str == "||") { /* 短路逻辑保持原样 */ }
            else {
                gen_expr_stack(e->left, fi, o, sp);
                gen_expr_stack(e->right, fi, o, sp);
                pop_to_t0(o, sp); emit(o, "mv t1, t0");
                pop_to_t0(o, sp);
                const string& op = e->op_str;
                if (e->left->type == Expr::IDENTIFIER && e->right->type == Expr::IDENTIFIER) {
                    int loff = fi.expr_resolved_offset.at(e->left);
                    int roff = fi.expr_resolved_offset.at(e->right);
                    string k = expr_key(op, loff, roff);
                    auto it = cse_map.find(k);
                    if (it != cse_map.end()) {
                        emit(o, "lw t0, " + to_string(it->second) + "(sp)");
                        push_reg_t0(o, sp); break;
                    }
                }
                if (op == "+") emit(o, "add t0, t0, t1");
                else if (op == "-") emit(o, "sub t0, t0, t1");
                else if (op == "*") emit(o, "mul t0, t0, t1");
                else if (op == "/") emit(o, "div t0, t0, t1");
                else if (op == "%") emit(o, "rem t0, t0, t1");
                else if (op == "<") emit(o, "slt t0, t0, t1");
                else if (op == ">") emit(o, "slt t0, t1, t0");
                else if (op == "<=") { emit(o, "slt t2, t1, t0"); emit(o, "xori t0, t2, 1"); }
                else if (op == ">=") { emit(o, "slt t2, t0, t1"); emit(o, "xori t0, t2, 1"); }
                else if (op == "==") { emit(o, "xor t2, t0, t1"); emit(o, "sltu t0, zero, t2"); emit(o, "xori t0, t0, 1"); }
                else if (op == "!=") { emit(o, "xor t2, t0, t1"); emit(o, "sltu t0, zero, t2"); }
                if (e->left->type == Expr::IDENTIFIER && e->right->type == Expr::IDENTIFIER) {
                    int loff = fi.expr_resolved_offset.at(e->left);
                    int roff = fi.expr_resolved_offset.at(e->right);
                    cse_map[expr_key(op, loff, roff)] = sp;
                }
                push_reg_t0(o, sp);
            } break;
        case Expr::FUNC_CALL:
            size_t argc = e->args.size();
            int total = argc * 4, pad = (16 - ((sp + total) & 15)) & 15;
            if (pad) { emit(o, "addi sp, sp, -" + to_string(pad)); sp += pad; }
            for (int i = argc - 1; i >= 0; --i) gen_expr_stack(e->args[i], fi, o, sp);
            for (size_t i = 0; i < min(argc, (size_t)8); ++i)
                emit(o, "lw a" + to_string(i) + ", " + to_string(i * 4) + "(sp)");
            emit(o, "call " + e->call_name);
            if (total + pad) { emit(o, "addi sp, sp, " + to_string(total + pad)); sp -= (total + pad); }
            emit(o, "mv t0, a0"); push_reg_t0(o, sp);
            break;
    }
}

/* ---------- 语句 ---------- */
static void gen_stmt(Stmt* s, const FuncInfo& fi, ostream& o, vector<pair<string, string>>& ls, int& sp) {
    if (!s) return;
    switch (s->type) {
        case Stmt::BLOCK: {
            unordered_map<string, int> save = cse_map;
            for (auto st : s->block_stmts) gen_stmt(st, fi, o, ls, sp);
            cse_map.swap(save);
            flush_peephole(o);
            break;
        }
        case Stmt::EMPTY: break;
        case Stmt::EXPR: gen_expr_stack(s->expr_stmt, fi, o, sp); pop_to_t0(o, sp); break;
        case Stmt::DECLARE:
            gen_expr_stack(s->declare_init, fi, o, sp); pop_to_t0(o, sp);
            emit(o, "sw t0, " + off_s0(fi.stmt_lhs_offset.at(s))); break;
        case Stmt::ASSIGN:
            gen_expr_stack(s->assign_rhs, fi, o, sp); pop_to_t0(o, sp);
            emit(o, "sw t0, " + off_s0(fi.stmt_lhs_offset.at(s))); break;
        case Stmt::IF: {
            string Lelse = lab.next("Lelse"), Lend = lab.next("Lend");
            gen_expr_stack(s->if_cond, fi, o, sp); pop_to_t0(o, sp);
            emit(o, "beqz t0, " + Lelse);
            gen_stmt(s->if_then, fi, o, ls, sp);
            emit(o, "j " + Lend);
            emit(o, Lelse + ":");
            if (s->if_else) gen_stmt(s->if_else, fi, o, ls, sp);
            emit(o, Lend + ":");
            flush_peephole(o);
            break;
        }
        case Stmt::WHILE: {
            string Lb = lab.next("Lwhile"), Le = lab.next("Lend");
            emit(o, Lb + ":");
            gen_expr_stack(s->while_cond, fi, o, sp); pop_to_t0(o, sp);
            emit(o, "beqz t0, " + Le);
            ls.emplace_back(Lb, Le);
            gen_stmt(s->while_body, fi, o, ls, sp);
            ls.pop_back();
            emit(o, "j " + Lb);
            emit(o, Le + ":");
            flush_peephole(o);
            break;
        }
        case Stmt::BREAK:    if (!ls.empty()) emit(o, "j " + ls.back().second); break;
        case Stmt::CONTINUE: if (!ls.empty()) emit(o, "j " + ls.back().first);  break;
        case Stmt::RETURN:
            if (s->return_expr) gen_expr_to_reg(s->return_expr, fi, o, sp, "a0");
            emit(o, "j __func_end_" + fi.name);
            flush_peephole(o);
            break;
    }
}

/* ---------- 主入口 ---------- */
bool generate_riscv(CompUnit* root, const vector<FuncInfo>& funcs, const string& out_path) {
    ostream* os = &cout; ofstream ofs;
    if (out_path != "-") { ofs.open(out_path); if (!ofs) return false; os = &ofs; }
    ostream& o = *os;
    for (size_t i = 0; i < root->funcs.size(); ++i) {
        FuncDef* f = root->funcs[i]; const FuncInfo& fi = funcs[i];
        int frame = align16(12 + 4 * ((int)fi.params.size() + fi.num_locals));
        emit(o, ".globl " + fi.name);
        emit(o, fi.name + ":");
        emit(o, "addi sp, sp, -" + to_string(frame));
        emit(o, "sw ra, " + to_string(frame - 4) + "(sp)");
        emit(o, "sw s0, " + to_string(frame - 8) + "(sp)");
        emit(o, "addi s0, sp, " + to_string(frame));
        for (size_t p = 0; p < fi.params.size(); ++p) {
            int off = fi.var_offset.at(fi.params[p]);
            if (p < 8) emit(o, "sw a" + to_string(p) + ", " + off_s0(off));
            else { emit(o, "lw t0, " + to_string((int)p * 4) + "(s0)"); emit(o, "sw t0, " + off_s0(off)); }
        }
        vector<pair<string, string>> ls; int cur_sp = 0;
        cse_map.clear();
        gen_stmt(f->body, fi, o, ls, cur_sp);
        emit(o, "__func_end_" + fi.name + ":");
        emit(o, "lw ra, " + to_string(frame - 4) + "(sp)");
        emit(o, "lw s0, " + to_string(frame - 8) + "(sp)");
        emit(o, "addi sp, sp, " + to_string(frame));
        emit(o, "jr ra");
        flush_peephole(o);
        if (ofs.is_open()) ofs.close();
    }
    return true;
}
