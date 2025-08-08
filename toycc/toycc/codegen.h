#pragma once
#include "ast.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <ostream>

class CodeGen {
public:
    explicit CodeGen(std::ostream& out);
    void generate(const std::vector<std::shared_ptr<FuncDef>>& funcs);

private:
    std::ostream& out;
    int labelCount = 0;
    int stackOffset = 0;
    std::unordered_map<std::string, int> varOffsets;
    std::vector<std::string> breakLabels;
    std::vector<std::string> continueLabels;

    void emit(const std::string& line);
    void genFunc(const std::shared_ptr<FuncDef>& func);
    //void genOtherFuncs(const std::vector<std::shared_ptr<FuncDef>>& funcs); // ÐÂÔöÉùÃ÷
    void genStmt(const std::shared_ptr<Stmt>& stmt);
    void genExpr(const std::shared_ptr<Expr>& expr, const std::string& dst);

    void genBlock(const std::shared_ptr<BlockStmt>& block);
    std::string genExprToReg(const std::shared_ptr<Expr>& expr);
    std::string newLabel(const std::string& base);

    void resetStack();
    void allocVar(const std::string& name);
};
