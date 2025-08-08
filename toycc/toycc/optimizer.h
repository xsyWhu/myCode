#pragma once
#include "ast.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <set>

class Optimizer {
public:
    void optimize(std::vector<std::shared_ptr<FuncDef>>& funcs);

private:
    void optimizeFunc(const std::shared_ptr<FuncDef>& func);
    void optimizeBlock(const std::shared_ptr<BlockStmt>& block,
        std::unordered_map<std::string, int>& constVars,
        bool inLoop = false);

    std::shared_ptr<Expr> optimizeExpr(const std::shared_ptr<Expr>& expr,
        std::unordered_map<std::string, int>& constVars,
        std::set<std::string>& loopInvariants);

    void hoistLoopInvariants(const std::shared_ptr<WhileStmt>& whileStmt,
        const std::unordered_map<std::string, int>& constVars);

    void eliminateDeadCode(const std::shared_ptr<BlockStmt>& block);
    void reduceStrength(const std::shared_ptr<BinaryExpr>& bin);

    bool isLoopInvariant(const std::shared_ptr<Expr>& expr,
        const std::set<std::string>& loopVars) const;

    void collectModifiedVars(const std::shared_ptr<Stmt>& stmt,
        std::set<std::string>& modifiedVars);

    void collectVarsInExpr(const std::shared_ptr<Expr>& expr,
        std::set<std::string>& vars);
};