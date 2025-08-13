#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include <string>
#include <vector>
#include <unordered_map>

/* 记录每个函数的语义信息（供 codegen 使用） */
struct FuncInfo {
    std::string name;
    std::string return_type; // "int" or "void"
    std::vector<std::string> params; // names in order
    int index_in_file = -1; // order in source
    int num_locals = 0; // number of local vars (not including params)
    // mapping variable name -> offset (relative to s0), used for declarations only
    std::unordered_map<std::string,int> var_offset;

    // mapping Expr* (IDENTIFIER usages) -> resolved offset
    std::unordered_map<const Expr*, int> expr_resolved_offset;

    // mapping Stmt* (ASSIGN or DECLARE statements) -> target offset
    std::unordered_map<const Stmt*, int> stmt_lhs_offset;
};

bool semantic_analyze(CompUnit* root, std::vector<FuncInfo>& out_funcs);

#endif // SEMANTIC_H
