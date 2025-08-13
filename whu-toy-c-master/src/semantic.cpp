#include "semantic.h"
#include <iostream>
#include <stack>
#include <cassert>
#include <unordered_set>

using namespace std;

/* Error helper */
static void sem_error(int line, const string &msg) {
    if (line > 0)
        fprintf(stderr, "Semantic error (line %d): %s\n", line, msg.c_str());
    else
        fprintf(stderr, "Semantic error: %s\n", msg.c_str());
}

/* forward */
static bool analyze_function(FuncDef* f, int func_index,
    const vector<FuncInfo>& global_funcs,
    FuncInfo& out_info, int cur_line_num);

/* main entry */
bool semantic_analyze(CompUnit* root, vector<FuncInfo>& out_funcs) {
    if (!root) {
        fprintf(stderr, "No AST to analyze\n");
        return false;
    }
    // 1. build global function list (names + signatures)
    for (size_t i = 0; i < root->funcs.size(); ++i) {
        FuncDef* f = root->funcs[i];
        // check uniqueness
        for (auto &fi : out_funcs) if (fi.name == f->name) {
            sem_error(0, "Duplicate function name: " + f->name);
            return false;
        }
        FuncInfo fi;
        fi.name = f->name;
        fi.return_type = f->return_type;
        fi.params = f->params;
        fi.index_in_file = (int)i;
        out_funcs.push_back(fi);
    }

    // check main exists and signature: int main()
    bool have_main = false;
    for (auto &fi : out_funcs) {
        if (fi.name == "main") {
            have_main = true;
            if (fi.return_type != "int" || fi.params.size() != 0) {
                sem_error(0, "main must be: int main()");
                return false;
            }
        }
    }
    if (!have_main) {
        sem_error(0, "missing entry function: int main()");
        return false;
    }

    // 2. per-function semantic checks (analyze in file order)
    for (size_t i = 0; i < root->funcs.size(); ++i) {
        FuncDef* f = root->funcs[i];
        FuncInfo tmp;
        if (!analyze_function(f, (int)i, out_funcs, tmp, 0)) return false;
        out_funcs[i] = std::move(tmp);
    }

    return true;
}

/* Resolve variable by scanning scope stack (top->bottom) */
static bool resolve_name_in_scopes(const string& name, const vector<unordered_map<string,int>>& scopes, int& out_offset) {
    for (int i = (int)scopes.size()-1; i >= 0; --i) {
        auto it = scopes[i].find(name);
        if (it != scopes[i].end()) {
            out_offset = it->second;
            return true;
        }
    }
    return false;
}

/* Conservative check whether a statement *always* returns (for int functions) */
static bool stmt_always_returns(Stmt* s) {
    if (!s) return false;
    switch (s->type) {
        case Stmt::RETURN: return true;
        case Stmt::BLOCK: {
            for (auto sub : s->block_stmts) {
                if (stmt_always_returns(sub)) return true; // 见注：简单保守策略
            }
            return false;
        }
        case Stmt::IF: {
            if (!s->if_else) return false;
            return stmt_always_returns(s->if_then) && stmt_always_returns(s->if_else);
        }
        case Stmt::WHILE:
            return false; // conservative
        default:
            return false;
    }
}

/* analyze_expr now has a flag allow_void_call_in_expr:
   - if false, any function call whose declared return type is "void" is an error here.
   - if true, void-call is allowed (this corresponds to top-level expression-statement).
*/
static bool analyze_expr(Expr* e, int cur_func_idx, const vector<FuncInfo>& global_funcs,
    vector<unordered_map<string,int>>& scopes, FuncInfo& myinfo, int &loop_depth, int cur_line,
    bool allow_void_call_in_expr) {

    if (!e) return true;
    switch (e->type) {
        case Expr::INT_CONST:
            return true;
        case Expr::IDENTIFIER: {
            int off = 0;
            if (!resolve_name_in_scopes(e->id_name, scopes, off)) {
                sem_error(cur_line, "use of undeclared variable: " + e->id_name);
                return false;
            }
            myinfo.expr_resolved_offset[e] = off;
            return true;
        }
        case Expr::UNARY_OP:
            return analyze_expr(e->child, cur_func_idx, global_funcs, scopes, myinfo, loop_depth, cur_line, allow_void_call_in_expr);
        case Expr::BINARY_OP: {
            if (!analyze_expr(e->left, cur_func_idx, global_funcs, scopes, myinfo, loop_depth, cur_line, allow_void_call_in_expr)) return false;
            if (!analyze_expr(e->right, cur_func_idx, global_funcs, scopes, myinfo, loop_depth, cur_line, allow_void_call_in_expr)) return false;
            return true;
        }
        case Expr::FUNC_CALL: {
            // find function in global_funcs
            int found = -1;
            for (size_t i = 0; i < global_funcs.size(); ++i) {
                if (global_funcs[i].name == e->call_name) { found = (int)i; break; }
            }
            if (found == -1) {
                sem_error(cur_line, "call to undefined function: " + e->call_name);
                return false;
            }
            // callee must be declared earlier or be self (support recursion)
            if (found > cur_func_idx) {
                sem_error(cur_line, "call to function declared later: " + e->call_name + " (declaration must appear before call)");
                return false;
            }
            // arg count match
            if (e->args.size() != global_funcs[found].params.size()) {
                sem_error(cur_line, "call argument count mismatch for " + e->call_name);
                return false;
            }
            // check each arg recursively; void-call rule applies only to containing expression context
            for (auto arg: e->args) {
                if (!analyze_expr(arg, cur_func_idx, global_funcs, scopes, myinfo, loop_depth, cur_line, true)) return false;
            }
            // if this call appears in an expression context that disallows void results, check it:
            if (!allow_void_call_in_expr && global_funcs[found].return_type == "void") {
                sem_error(cur_line, "void function '" + e->call_name + "' used in expression context");
                return false;
            }
            return true;
        }
    }
    return true;
}

/* Analyze statements.
   cur_ret_type: function return type ("int" or "void").
*/
static bool analyze_stmt(Stmt* s, int cur_func_idx, const vector<FuncInfo>& global_funcs,
    vector<unordered_map<string,int>>& scopes, FuncInfo& myinfo, int &next_local_index, int &loop_depth, int cur_line,
    const string &cur_ret_type) {

    if (!s) return true;
    switch (s->type) {
        case Stmt::BLOCK: {
            scopes.emplace_back(); // new inner scope
            for (auto sub : s->block_stmts) {
                if (!analyze_stmt(sub, cur_func_idx, global_funcs, scopes, myinfo, next_local_index, loop_depth, cur_line, cur_ret_type)) return false;
            }
            scopes.pop_back();
            return true;
        }
        case Stmt::EMPTY:
            return true;
        case Stmt::EXPR:
            // expression-statement allows void function calls (side-effect); mark allow_void=true
            return analyze_expr(s->expr_stmt, cur_func_idx, global_funcs, scopes, myinfo, loop_depth, cur_line, true);
        case Stmt::DECLARE: {
            if (!s->declare_init) {
                sem_error(cur_line, "variable declaration must have initializer for: " + s->declare_id);
                return false;
            }
            auto &top = scopes.back();
            if (top.find(s->declare_id) != top.end()) {
                sem_error(cur_line, "redeclaration in same scope: " + s->declare_id);
                return false;
            }
            int assigned_index = next_local_index++;
            int offset = -12 - 4 * assigned_index; // relative to s0
            top[s->declare_id] = offset;
            myinfo.var_offset[s->declare_id] = offset;
            myinfo.stmt_lhs_offset[s] = offset;
            if (!analyze_expr(s->declare_init, cur_func_idx, global_funcs, scopes, myinfo, loop_depth, cur_line, false)) return false;
            return true;
        }
        case Stmt::ASSIGN: {
            if (!analyze_expr(s->assign_rhs, cur_func_idx, global_funcs, scopes, myinfo, loop_depth, cur_line, false)) return false;
            int off = 0;
            if (!resolve_name_in_scopes(s->assign_id, scopes, off)) {
                sem_error(cur_line, "assignment to undeclared variable: " + s->assign_id);
                return false;
            }
            myinfo.stmt_lhs_offset[s] = off;
            return true;
        }
        case Stmt::IF: {
            if (!analyze_expr(s->if_cond, cur_func_idx, global_funcs, scopes, myinfo, loop_depth, cur_line, false)) return false;
            if (!analyze_stmt(s->if_then, cur_func_idx, global_funcs, scopes, myinfo, next_local_index, loop_depth, cur_line, cur_ret_type)) return false;
            if (s->if_else) {
                if (!analyze_stmt(s->if_else, cur_func_idx, global_funcs, scopes, myinfo, next_local_index, loop_depth, cur_line, cur_ret_type)) return false;
            }
            return true;
        }
        case Stmt::WHILE: {
            if (!analyze_expr(s->while_cond, cur_func_idx, global_funcs, scopes, myinfo, loop_depth, cur_line, false)) return false;
            loop_depth++;
            bool ok = analyze_stmt(s->while_body, cur_func_idx, global_funcs, scopes, myinfo, next_local_index, loop_depth, cur_line, cur_ret_type);
            loop_depth--;
            return ok;
        }
        case Stmt::BREAK:
            if (loop_depth <= 0) {
                sem_error(cur_line, "break used outside of loop");
                return false;
            }
            return true;
        case Stmt::CONTINUE:
            if (loop_depth <= 0) {
                sem_error(cur_line, "continue used outside of loop");
                return false;
            }
            return true;
        case Stmt::RETURN:
            if (s->return_expr) {
                if (cur_ret_type == "void") {
                    sem_error(cur_line, "return with a value in void function");
                    return false;
                }
                if (!analyze_expr(s->return_expr, cur_func_idx, global_funcs, scopes, myinfo, loop_depth, cur_line, false)) return false;
            } else {
                // "return;" form
                if (cur_ret_type == "int") {
                    sem_error(cur_line, "missing return value in int function");
                    return false;
                }
            }
            return true;
        default:
            return true;
    }
}

/* analyze a single function */
static bool analyze_function(FuncDef* f, int func_index,
    const vector<FuncInfo>& global_funcs,
    FuncInfo& out_info, int cur_line_num) {

    out_info.name = f->name;
    out_info.return_type = f->return_type;
    out_info.params = f->params;
    out_info.index_in_file = func_index;

    // create scopes stack; first scope holds params
    vector<unordered_map<string,int>> scopes;
    scopes.emplace_back();

    // param offsets
    int next_local_index = 0;
    for (size_t i = 0; i < f->params.size(); ++i) {
        int assigned_index = next_local_index++;
        int offset = -12 - 4 * assigned_index;
        scopes.back()[f->params[i]] = offset;
        out_info.var_offset[f->params[i]] = offset;
    }

    int loop_depth = 0;

    // analyze body
    if (!analyze_stmt(f->body, func_index, global_funcs, scopes, out_info, next_local_index, loop_depth, cur_line_num, f->return_type)) {
        return false;
    }

    out_info.num_locals = next_local_index - (int)f->params.size();

    // For int functions, ensure (conservatively) that all paths return
    if (f->return_type == "int") {
        if (!stmt_always_returns(f->body)) {
            sem_error(0, "int function '" + f->name + "' may not return on every path");
            return false;
        }
    }

    return true;
}
