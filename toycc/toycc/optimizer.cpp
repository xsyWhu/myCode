#include "optimizer.h"
#include <stdexcept>
#include <iostream>

void Optimizer::optimize(std::vector<std::shared_ptr<FuncDef>>& funcs) {
    for (auto& func : funcs) {
        optimizeFunc(func);
    }
}

void Optimizer::optimizeFunc(const std::shared_ptr<FuncDef>& func) {
    std::unordered_map<std::string, int> constVars;
    optimizeBlock(func->body, constVars);
}

void Optimizer::optimizeBlock(const std::shared_ptr<BlockStmt>& block,
    std::unordered_map<std::string, int>& constVars,
    bool inLoop) {
    // 复制当前作用域的常量
    std::unordered_map<std::string, int> currentConstVars = constVars;

    for (auto it = block->statements.begin(); it != block->statements.end();) {
        auto& stmt = *it;

        // 死代码消除
        if (auto ret = std::dynamic_pointer_cast<ReturnStmt>(stmt)) {
            // 删除return之后的语句
            block->statements.erase(++it, block->statements.end());
            break;
        }

        // 声明语句优化
        if (auto decl = std::dynamic_pointer_cast<DeclareStmt>(stmt)) {
            // 优化初始化表达式
            std::set<std::string> loopVars;
            decl->initVal = optimizeExpr(decl->initVal, currentConstVars, loopVars);

            // 如果是常量，加入常量表
            if (auto num = std::dynamic_pointer_cast<NumberExpr>(decl->initVal)) {
                currentConstVars[decl->varName] = num->value;
            }
            else {
                currentConstVars.erase(decl->varName);
            }
            ++it;
        }
        // 赋值语句优化
        else if (auto assign = std::dynamic_pointer_cast<AssignStmt>(stmt)) {
            // 优化右值表达式
            std::set<std::string> loopVars;
            assign->value = optimizeExpr(assign->value, currentConstVars, loopVars);

            // 强度削弱
            if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(assign->value)) {
                reduceStrength(bin);
            }

            // 从常量表中移除（值已改变）
            currentConstVars.erase(assign->varName);
            ++it;
        }
        // 循环语句优化
        else if (auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(stmt)) {
            // 收集循环体中修改的变量
            std::set<std::string> modifiedVars;
            collectModifiedVars(whileStmt->body, modifiedVars);

            // 从常量表中移除可能被修改的变量
            for (const auto& var : modifiedVars) {
                currentConstVars.erase(var);
            }

            // 收集循环条件中的变量
            std::set<std::string> condVars;
            collectVarsInExpr(whileStmt->condition, condVars);

            // 合并变量
            std::set<std::string> loopVars;
            loopVars.insert(modifiedVars.begin(), modifiedVars.end());
            loopVars.insert(condVars.begin(), condVars.end());

            // 优化条件表达式
            std::set<std::string> loopInvariants;
            whileStmt->condition = optimizeExpr(whileStmt->condition, currentConstVars, loopInvariants);

            // 仅当循环条件不是常量时才尝试提升不变式
            if (!std::dynamic_pointer_cast<NumberExpr>(whileStmt->condition)) {
                hoistLoopInvariants(whileStmt, currentConstVars);
            }

            // 递归优化循环体
            bool oldInLoop = inLoop;
            inLoop = true;
            if (auto bodyBlock = std::dynamic_pointer_cast<BlockStmt>(whileStmt->body)) {
                optimizeBlock(bodyBlock, currentConstVars, true);
            }
            else {
                // 将非块语句转换为块语句
                auto newBody = std::make_shared<BlockStmt>();
                newBody->statements.push_back(whileStmt->body);
                optimizeBlock(newBody, currentConstVars, true);
                whileStmt->body = newBody;
            }
            inLoop = oldInLoop;
            ++it;
        }
        // 其他语句优化
        else {
            if (auto ifStmt = std::dynamic_pointer_cast<IfStmt>(stmt)) {
                // 优化条件表达式
                std::set<std::string> loopVars;
                ifStmt->condition = optimizeExpr(ifStmt->condition, currentConstVars, loopVars);

                // 死代码消除：条件为常量
                if (auto num = std::dynamic_pointer_cast<NumberExpr>(ifStmt->condition)) {
                    if (num->value != 0) {
                        // 条件为真，替换为then语句
                        stmt = ifStmt->thenStmt;
                    }
                    else if (ifStmt->elseStmt) {
                        // 条件为假，替换为else语句
                        stmt = ifStmt->elseStmt;
                    }
                    else {
                        // 条件为假且无else，删除整个if语句
                        it = block->statements.erase(it);
                        continue;
                    }
                }
            }
            else if (auto exprStmt = std::dynamic_pointer_cast<ExprStmt>(stmt)) {
                // 优化表达式
                std::set<std::string> loopVars;
                exprStmt->expr = optimizeExpr(exprStmt->expr, currentConstVars, loopVars);

                // 如果表达式是常量，删除该语句
                if (std::dynamic_pointer_cast<NumberExpr>(exprStmt->expr)) {
                    it = block->statements.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }

    // 块级别的死代码消除
    eliminateDeadCode(block);

    // 修复: 确保块语句的语句列表是有效的
    if (block) {
        for (auto& stmt : block->statements) {
            if (auto subBlock = std::dynamic_pointer_cast<BlockStmt>(stmt)) {
                optimizeBlock(subBlock, constVars, inLoop);
            }
        }
    }
}

std::shared_ptr<Expr> Optimizer::optimizeExpr(const std::shared_ptr<Expr>& expr,
    std::unordered_map<std::string, int>& constVars,
    std::set<std::string>& loopInvariants) {
    if (!expr) return expr;

    // 常量传播：变量替换为常量
    if (auto var = std::dynamic_pointer_cast<VariableExpr>(expr)) {
        auto it = constVars.find(var->name);
        if (it != constVars.end()) {
            return std::make_shared<NumberExpr>(it->second);
        }
        loopInvariants.insert(var->name);
        return var;
    }

    // 二元表达式优化
    if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
        bin->lhs = optimizeExpr(bin->lhs, constVars, loopInvariants);
        bin->rhs = optimizeExpr(bin->rhs, constVars, loopInvariants);

        // 常量折叠
        if (auto lhsNum = std::dynamic_pointer_cast<NumberExpr>(bin->lhs)) {
            if (auto rhsNum = std::dynamic_pointer_cast<NumberExpr>(bin->rhs)) {
                int left = lhsNum->value;
                int right = rhsNum->value;
                int result = 0;

                if (bin->op == "+") result = left + right;
                else if (bin->op == "-") result = left - right;
                else if (bin->op == "*") result = left * right;
                else if (bin->op == "/" && right != 0) result = left / right;
                else if (bin->op == "%" && right != 0) result = left % right;
                else if (bin->op == "<") result = left < right;
                else if (bin->op == ">") result = left > right;
                else if (bin->op == "<=") result = left <= right;
                else if (bin->op == ">=") result = left >= right;
                else if (bin->op == "==") result = left == right;
                else if (bin->op == "!=") result = left != right;
                else if (bin->op == "&&") result = left && right;
                else if (bin->op == "||") result = left || right;
                else return bin;

                return std::make_shared<NumberExpr>(result);
            }
        }
        return bin;
    }

    // 函数调用优化
    if (auto call = std::dynamic_pointer_cast<CallExpr>(expr)) {
        for (auto& arg : call->args) {
            arg = optimizeExpr(arg, constVars, loopInvariants);
        }
        return call;
    }

    return expr;
}

void Optimizer::hoistLoopInvariants(const std::shared_ptr<WhileStmt>& whileStmt,
    const std::unordered_map<std::string, int>& constVars) {
    if (!whileStmt->body) return;

    // 收集循环中可能被修改的变量
    std::set<std::string> modifiedVars;
    collectModifiedVars(whileStmt->body, modifiedVars);

    // 收集循环条件中的变量
    std::set<std::string> condVars;
    collectVarsInExpr(whileStmt->condition, condVars);

    // 合并变量
    std::set<std::string> loopVars;
    loopVars.insert(modifiedVars.begin(), modifiedVars.end());
    loopVars.insert(condVars.begin(), condVars.end());

    // 确保循环体是块语句
    auto bodyBlock = std::dynamic_pointer_cast<BlockStmt>(whileStmt->body);
    if (!bodyBlock) {
        // 如果循环体不是块语句，将其转换为块语句
        auto newBody = std::make_shared<BlockStmt>();
        newBody->statements.push_back(whileStmt->body);
        whileStmt->body = newBody;
        bodyBlock = newBody;
    }

    // 提取循环不变式
    auto& stmts = bodyBlock->statements;
    std::vector<std::shared_ptr<Stmt>> hoistedStmts;

    for (auto it = stmts.begin(); it != stmts.end(); ) {
        bool hoist = false;

        if (auto assign = std::dynamic_pointer_cast<AssignStmt>(*it)) {
            // 检查赋值右侧是否是循环不变式
            if (isLoopInvariant(assign->value, loopVars)) {
                hoist = true;
            }
        }
        else if (auto decl = std::dynamic_pointer_cast<DeclareStmt>(*it)) {
            // 检查初始化表达式是否是循环不变式
            if (isLoopInvariant(decl->initVal, loopVars)) {
                hoist = true;
            }
        }

        if (hoist) {
            // 添加到提升列表
            hoistedStmts.push_back(*it);
            it = stmts.erase(it);
        }
        else {
            ++it;
        }
    }

    // 如果有提升的语句，创建新的循环体
    if (!hoistedStmts.empty()) {
        // 创建新的循环体（包含提升的语句和原循环体）
        auto newBody = std::make_shared<BlockStmt>();
        newBody->statements = hoistedStmts;
        newBody->statements.push_back(bodyBlock);
        whileStmt->body = newBody;
    }
}

bool Optimizer::isLoopInvariant(const std::shared_ptr<Expr>& expr,
    const std::set<std::string>& loopVars) const {
    if (!expr) return true;

    if (auto var = std::dynamic_pointer_cast<VariableExpr>(expr)) {
        // 如果表达式包含循环变量，则不是循环不变式
        return loopVars.find(var->name) == loopVars.end();
    }

    if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
        return isLoopInvariant(bin->lhs, loopVars) &&
            isLoopInvariant(bin->rhs, loopVars);
    }

    if (auto call = std::dynamic_pointer_cast<CallExpr>(expr)) {
        // 假设函数调用不是循环不变式（保守策略）
        return false;
    }

    return true;
}

void Optimizer::eliminateDeadCode(const std::shared_ptr<BlockStmt>& block) {
    if (!block) return;

    for (auto it = block->statements.begin(); it != block->statements.end(); ) {
        if (auto ifStmt = std::dynamic_pointer_cast<IfStmt>(*it)) {
            // 递归消除死代码
            if (auto thenBlock = std::dynamic_pointer_cast<BlockStmt>(ifStmt->thenStmt)) {
                eliminateDeadCode(thenBlock);
            }
            else if (ifStmt->thenStmt) {
                // 如果不是块语句，转换为块语句
                auto newThenBlock = std::make_shared<BlockStmt>();
                newThenBlock->statements.push_back(ifStmt->thenStmt);
                ifStmt->thenStmt = newThenBlock;
                eliminateDeadCode(newThenBlock);
            }

            if (ifStmt->elseStmt) {
                if (auto elseBlock = std::dynamic_pointer_cast<BlockStmt>(ifStmt->elseStmt)) {
                    eliminateDeadCode(elseBlock);
                }
                else {
                    // 如果不是块语句，转换为块语句
                    auto newElseBlock = std::make_shared<BlockStmt>();
                    newElseBlock->statements.push_back(ifStmt->elseStmt);
                    ifStmt->elseStmt = newElseBlock;
                    eliminateDeadCode(newElseBlock);
                }
            }
            ++it;
        }
        else if (auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(*it)) {
            if (auto bodyBlock = std::dynamic_pointer_cast<BlockStmt>(whileStmt->body)) {
                eliminateDeadCode(bodyBlock);
            }
            else if (whileStmt->body) {
                // 如果不是块语句，转换为块语句
                auto newBodyBlock = std::make_shared<BlockStmt>();
                newBodyBlock->statements.push_back(whileStmt->body);
                whileStmt->body = newBodyBlock;
                eliminateDeadCode(newBodyBlock);
            }
            ++it;
        }
        else {
            ++it;
        }
    }
}

void Optimizer::reduceStrength(const std::shared_ptr<BinaryExpr>& bin) {
    if (!bin) return;

    // 乘法转移位
    if (bin->op == "*") {
        if (auto rhsNum = std::dynamic_pointer_cast<NumberExpr>(bin->rhs)) {
            int val = rhsNum->value;
            if (val > 0 && (val & (val - 1)) == 0) { // 判断是否为2的幂
                int shift = 0;
                while (val > 1) {
                    val >>= 1;
                    shift++;
                }
                bin->op = "<<";
                bin->rhs = std::make_shared<NumberExpr>(shift);
            }
        }
    }
}

void Optimizer::collectModifiedVars(const std::shared_ptr<Stmt>& stmt,
    std::set<std::string>& modifiedVars) {
    if (!stmt) return;

    if (auto block = std::dynamic_pointer_cast<BlockStmt>(stmt)) {
        for (auto& s : block->statements) {
            collectModifiedVars(s, modifiedVars);
        }
    }
    else if (auto assign = std::dynamic_pointer_cast<AssignStmt>(stmt)) {
        modifiedVars.insert(assign->varName);
    }
    else if (auto decl = std::dynamic_pointer_cast<DeclareStmt>(stmt)) {
        modifiedVars.insert(decl->varName);
    }
    else if (auto ifStmt = std::dynamic_pointer_cast<IfStmt>(stmt)) {
        collectModifiedVars(ifStmt->thenStmt, modifiedVars);
        if (ifStmt->elseStmt) {
            collectModifiedVars(ifStmt->elseStmt, modifiedVars);
        }
    }
    else if (auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(stmt)) {
        collectModifiedVars(whileStmt->body, modifiedVars);
    }
}

void Optimizer::collectVarsInExpr(const std::shared_ptr<Expr>& expr,
    std::set<std::string>& vars) {
    if (!expr) return;

    if (auto var = std::dynamic_pointer_cast<VariableExpr>(expr)) {
        vars.insert(var->name);
    }
    else if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
        collectVarsInExpr(bin->lhs, vars);
        collectVarsInExpr(bin->rhs, vars);
    }
    else if (auto call = std::dynamic_pointer_cast<CallExpr>(expr)) {
        for (const auto& arg : call->args) {
            collectVarsInExpr(arg, vars);
        }
    }
}