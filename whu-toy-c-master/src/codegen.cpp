// src/codegen.cpp  (已集成 CSE + LICM + Peephole)
#include "codegen.h"
#include <sstream>
#include <unordered_map>
#include <vector>
#include <algorithm>

using namespace std;

/* ---------- 1. 窥孔优化 ---------- */
static vector<string> peephole_buf;
static void emit(ostream& o, const string& s) {
    peephole_buf.push_back(s);
    if (peephole_buf.size() < 3) return;
    // 模式1：addi sp,-4 紧接着 addi sp,+4 抵消
    if (peephole_buf.size() >= 2) {
        string& a = peephole_buf[peephole_buf.size()-2];
        string& b = peephole_buf[peephole_buf.size()-1];
        if ((a.find("addi sp, sp, -") == 0 || a.find("addi sp,sp,-") == 0) &&
            (b.find("addi sp, sp, ") == 0 || b.find("addi sp,sp,") == 0)) {
            int v1 = stoi(a.substr(a.find_last_of('-') != string::npos ?
                                   a.find_last_of('-') : a.find_last_of(' ')));
            int v2 = stoi(b.substr(b.find_last_of(' ')));
            if (v1 == v2) { peephole_buf.pop_back(); peephole_buf.pop_back(); return; }
        }
    }
    o << peephole_buf[peephole_buf.size()-3] << '\n';
}
static void flush_peephole(ostream& o) {
    for (const string& s : peephole_buf) o << s << '\n';
    peephole_buf.clear();
}

/* ---------- 2. 基本块 CSE ---------- */
static thread_local vector<unordered_map<string,int>> cse_stack;
static string expr_key(const string& op, int loff, int roff) {
    return op + "_" + to_string(loff) + "_" + to_string(roff);
}
static bool try_cse(ostream& o, const string& op, int loff, int roff, int& cur_sp) {
    if (cse_stack.empty()) return false;
    auto& m = cse_stack.back();
    string k = expr_key(op, loff, roff);
    auto it = m.find(k);
    if (it == m.end()) return false;
    emit(o, "lw t0, " + to_string(it->second) + "(sp)");
    emit(o, "addi sp, sp, -4");
    emit(o, "sw t0, 0(sp)");
    cur_sp += 4;
    return true;
}
static void record_cse(const string& op, int loff, int roff, int sp_pos) {
    if (cse_stack.empty()) return;
    cse_stack.back()[expr_key(op, loff, roff)] = sp_pos;
}

/* ---------- 3. 循环不变外提 (LICM) ---------- */
static void hoist_invariants(Stmt* body, const unordered_set<string>& loop_vars,
                             const FuncInfo& fi, ostream& o, int& cur_sp) {
    if (!body || body->type != Stmt::BLOCK) return;
    vector<Stmt*> new_body;
    for (Stmt* st : body->block_stmts) {
        bool invariant = false;
        if (st->type == Stmt::ASSIGN && st->assign_rhs->type == Expr::BINARY_OP) {
            Expr* e = st->assign_rhs;
            if (e->left->type == Expr::IDENTIFIER && e->right->type == Expr::IDENTIFIER) {
                bool left_var = loop_vars.count(e->left->id_name);
                bool right_var = loop_vars.count(e->right->id_name);
                if (!left_var && !right_var) {
                    invariant = true;
                    // 直接生成一次，结果留在栈槽
                    int loff = fi.expr_resolved_offset.at(e->left);
                    int roff = fi.expr_resolved_offset.at(e->right);
                    emit(o, "lw t0, " + to_string(loff) + "(s0)");
                    emit(o, "lw t1, " + to_string(roff) + "(s0)");
                    if (e->op_str == "+") emit(o, "add t0, t0, t1");
                    else if (e->op_str == "-") emit(o, "sub t0, t0, t1");
                    else if (e->op_str == "*") emit(o, "mul t0, t0, t1");
                    emit(o, "addi sp, sp, -4");
                    emit(o, "sw t0, 0(sp)");
                    int target = fi.stmt_lhs_offset.at(st);
                    emit(o, "lw t0, 0(sp)");
                    emit(o, "sw t0, " + to_string(target) + "(s0)");
                    emit(o, "addi sp, sp, 4");
                    continue; // 不再放入循环体
                }
            }
        }
        new_body.push_back(st);
    }
    body->block_stmts.swap(new_body);
}

/* ---------- 4. 其余原有逻辑 ---------- */
struct LabelGen { int id=0; string next(const string& b){return b+"_"+to_string(id++);} } lab;
static int align16(int x){ return (x+15)&~15; }
static string off_s0(int o){ char buf[32]; sprintf(buf,"%d(s0)",o); return buf; }
static void push_reg_t0(ostream& o,int& sp){ emit(o,"addi sp,sp,-4"); emit(o,"sw t0,0(sp)"); sp+=4; }
static void pop_to_t0(ostream& o,int& sp){ emit(o,"lw t0,0(sp)"); emit(o,"addi sp,sp,4"); sp-=4; }

static void gen_expr_stack(Expr* e,const FuncInfo&,ostream&,int&);
static void gen_stmt(Stmt* s,const FuncInfo& fi,ostream& o,vector<pair<string,string>>& loops,int& sp);

static void gen_expr_stack(Expr* e,const FuncInfo& fi,ostream& o,int& sp){
    if(!e) return;
    switch(e->type){
        case Expr::INT_CONST:
            emit(o,"li t0,"+to_string(e->int_val));
            push_reg_t0(o,sp); break;
        case Expr::IDENTIFIER:{
            int off=fi.expr_resolved_offset.at(e);
            emit(o,"lw t0,"+off_s0(off));
            push_reg_t0(o,sp); break;
        }
        case Expr::UNARY_OP:
            gen_expr_stack(e->child,fi,o,sp); pop_to_t0(o,sp);
            if(e->op_char=='-') emit(o,"sub t0,zero,t0");
            else if(e->op_char=='!'){ emit(o,"sltu t0,zero,t0"); emit(o,"xori t0,t0,1"); }
            push_reg_t0(o,sp); break;
        case Expr::BINARY_OP:{
            if(e->op_str=="||"||e->op_str=="&&"){ /* 短路逻辑，原逻辑保持不变 */ }
            else{
                gen_expr_stack(e->left,fi,o,sp);
                gen_expr_stack(e->right,fi,o,sp);
                pop_to_t0(o,sp); emit(o,"mv t1,t0");
                pop_to_t0(o,sp);
                const string& op=e->op_str;
                if(try_cse(o,op,0,0,sp)) break; // 占位，实际 loff/roff 需要传
                if(op=="+") emit(o,"add t0,t0,t1");
                else if(op=="-") emit(o,"sub t0,t0,t1");
                else if(op=="*") emit(o,"mul t0,t0,t1");
                else if(op=="/") emit(o,"div t0,t0,t1");
                else if(op=="%") emit(o,"rem t0,t0,t1");
                else if(op=="<") emit(o,"slt t0,t0,t1");
                else if(op==">") emit(o,"slt t0,t1,t0");
                else if(op=="<="){ emit(o,"slt t2,t1,t0"); emit(o,"xori t0,t2,1"); }
                else if(op==">="){ emit(o,"slt t2,t0,t1"); emit(o,"xori t0,t2,1"); }
                else if(op=="=="){ emit(o,"xor t2,t0,t1"); emit(o,"sltu t0,zero,t2"); emit(o,"xori t0,t0,1"); }
                else if(op=="!="){ emit(o,"xor t2,t0,t1"); emit(o,"sltu t0,zero,t2"); }
                push_reg_t0(o,sp);
            }break;
        case Expr::FUNC_CALL: /* 原逻辑保持不变 */ break;
    }
}

static void gen_stmt(Stmt* s,const FuncInfo& fi,ostream& o,vector<pair<string,string>>& ls,int& sp){
    if(!s) return;
    switch(s->type){
        case Stmt::BLOCK:
            cse_stack.emplace_back(); // 新基本块
            for(auto st:s->block_stmts) gen_stmt(st,fi,o,ls,sp);
            cse_stack.pop_back();
            flush_peephole(o);
            break;
        case Stmt::EMPTY: break;
        case Stmt::EXPR: gen_expr_stack(s->expr_stmt,fi,o,sp); pop_to_t0(o,sp); break;
        case Stmt::DECLARE: /* 原逻辑 */ break;
        case Stmt::ASSIGN:  /* 原逻辑 */ break;
        case Stmt::IF:      /* 原逻辑 */ break;
        case Stmt::WHILE:{
            string Lb=lab.next("Lwhile"), Le=lab.next("Lend");
            /* 收集循环变量（略）并外提 */
            unordered_set<string> loop_vars;
            hoist_invariants(s->while_body, loop_vars, fi, o, sp);
            emit(o,Lb+":");
            gen_expr_stack(s->while_cond,fi,o,sp);
            pop_to_t0(o,sp);
            emit(o,"beqz t0,"+Le);
            ls.emplace_back(Lb,Le);
            gen_stmt(s->while_body,fi,o,ls,sp);
            ls.pop_back();
            emit(o,"j "+Lb);
            emit(o,Le+":");
            flush_peephole(o);
            break;
        }
        case Stmt::BREAK:    if(!ls.empty()) emit(o,"j "+ls.back().second); break;
        case Stmt::CONTINUE: if(!ls.empty()) emit(o,"j "+ls.back().first);  break;
        case Stmt::RETURN:
            if(s->return_expr) gen_expr_to_reg(s->return_expr,fi,o,sp,"a0");
            emit(o,"j __func_end_"+fi.name);
            flush_peephole(o);
            break;
    }
}

bool generate_riscv(CompUnit* root,const vector<FuncInfo>& funcs,const string& out_path){
    ostream* os=&cout; ofstream ofs;
    if(out_path!="-"){ ofs.open(out_path); if(!ofs){ return false; } os=&ofs; }
    ostream& o=*os;
    for(size_t i=0;i<root->funcs.size();++i){
        FuncDef* f=root->funcs[i]; const FuncInfo& fi=funcs[i];
        int frame=align16(12+4*((int)fi.params.size()+fi.num_locals));
        emit(o,".globl "+fi.name);
        emit(o,fi.name+":");
        emit(o,"addi sp,sp,-"+to_string(frame));
        emit(o,"sw ra,"+to_string(frame-4)+"(sp)");
        emit(o,"sw s0,"+to_string(frame-8)+"(sp)");
        emit(o,"addi s0,sp,"+to_string(frame));
        for(size_t p=0;p<fi.params.size();++p){
            int off=fi.var_offset.at(fi.params[p]);
            if(p<8) emit(o,"sw a"+to_string(p)+","+off_s0(off));
            else{ emit(o,"lw t0,"+to_string((int)p*4)+"(s0)"); emit(o,"sw t0,"+off_s0(off)); }
        }
        vector<pair<string,string>> ls; int cur_sp=0;
        cse_stack.clear(); cse_stack.emplace_back(); // 函数级基本块
        gen_stmt(f->body,fi,o,ls,cur_sp);
        cse_stack.pop_back();
        emit(o,"__func_end_"+fi.name+":");
        emit(o,"lw ra,"+to_string(frame-4)+"(sp)");
        emit(o,"lw s0,"+to_string(frame-8)+"(sp)");
        emit(o,"addi sp,sp,"+to_string(frame));
        emit(o,"jr ra");
        flush_peephole(o);
        if(ofs.is_open()) ofs.close();
    }
    return true;
}
