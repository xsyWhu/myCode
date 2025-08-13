#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "semantic.h"
#include <string>

bool generate_riscv(CompUnit* root, const std::vector<FuncInfo>& funcs, const std::string& out_path);

#endif // CODEGEN_H
