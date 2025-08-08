#pragma once
#include "ast.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <stack>
#include <stdexcept>

class SemanticAnalyzer {
public:
    void analyze(const std::vector<std::shared_ptr<FuncDef>>& funcs);

private:
    struct VarInfo {
        std::string type;
        bool isDeclared = false;
    };

    std::unordered_map<std::string, std::string> funcTable;
    std::stack<std::unordered_map<std::string, VarInfo>> varScopes;

    std::string currentFuncRetType;
    bool inLoop = false;

    void enterScope();
    void exitScope();
    void declareVar(const std::string& name, const std::string& type);
    bool isVarDeclared(const std::string& name);

    void checkFunc(const std::shared_ptr<FuncDef>& func);
    void checkStmt(const std::shared_ptr<Stmt>& stmt);
    void checkExpr(const std::shared_ptr<Expr>& expr);

    // class BreakStmt;
    // class ContinueStmt;
};
