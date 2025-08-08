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
    // ���Ƶ�ǰ������ĳ���
    std::unordered_map<std::string, int> currentConstVars = constVars;

    for (auto it = block->statements.begin(); it != block->statements.end();) {
        auto& stmt = *it;

        // ����������
        if (auto ret = std::dynamic_pointer_cast<ReturnStmt>(stmt)) {
            // ɾ��return֮������
            block->statements.erase(++it, block->statements.end());
            break;
        }

        // ��������Ż�
        if (auto decl = std::dynamic_pointer_cast<DeclareStmt>(stmt)) {
            // �Ż���ʼ�����ʽ
            std::set<std::string> loopVars;
            decl->initVal = optimizeExpr(decl->initVal, currentConstVars, loopVars);

            // ����ǳ��������볣����
            if (auto num = std::dynamic_pointer_cast<NumberExpr>(decl->initVal)) {
                currentConstVars[decl->varName] = num->value;
            }
            else {
                currentConstVars.erase(decl->varName);
            }
            ++it;
        }
        // ��ֵ����Ż�
        else if (auto assign = std::dynamic_pointer_cast<AssignStmt>(stmt)) {
            // �Ż���ֵ���ʽ
            std::set<std::string> loopVars;
            assign->value = optimizeExpr(assign->value, currentConstVars, loopVars);

            // ǿ������
            if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(assign->value)) {
                reduceStrength(bin);
            }

            // �ӳ��������Ƴ���ֵ�Ѹı䣩
            currentConstVars.erase(assign->varName);
            ++it;
        }
        // ѭ������Ż�
        else if (auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(stmt)) {
            // �ռ�ѭ�������޸ĵı���
            std::set<std::string> modifiedVars;
            collectModifiedVars(whileStmt->body, modifiedVars);

            // �ӳ��������Ƴ����ܱ��޸ĵı���
            for (const auto& var : modifiedVars) {
                currentConstVars.erase(var);
            }

            // �ռ�ѭ�������еı���
            std::set<std::string> condVars;
            collectVarsInExpr(whileStmt->condition, condVars);

            // �ϲ�����
            std::set<std::string> loopVars;
            loopVars.insert(modifiedVars.begin(), modifiedVars.end());
            loopVars.insert(condVars.begin(), condVars.end());

            // �Ż��������ʽ
            std::set<std::string> loopInvariants;
            whileStmt->condition = optimizeExpr(whileStmt->condition, currentConstVars, loopInvariants);

            // ����ѭ���������ǳ���ʱ�ų�����������ʽ
            if (!std::dynamic_pointer_cast<NumberExpr>(whileStmt->condition)) {
                hoistLoopInvariants(whileStmt, currentConstVars);
            }

            // �ݹ��Ż�ѭ����
            bool oldInLoop = inLoop;
            inLoop = true;
            if (auto bodyBlock = std::dynamic_pointer_cast<BlockStmt>(whileStmt->body)) {
                optimizeBlock(bodyBlock, currentConstVars, true);
            }
            else {
                // ���ǿ����ת��Ϊ�����
                auto newBody = std::make_shared<BlockStmt>();
                newBody->statements.push_back(whileStmt->body);
                optimizeBlock(newBody, currentConstVars, true);
                whileStmt->body = newBody;
            }
            inLoop = oldInLoop;
            ++it;
        }
        // ��������Ż�
        else {
            if (auto ifStmt = std::dynamic_pointer_cast<IfStmt>(stmt)) {
                // �Ż��������ʽ
                std::set<std::string> loopVars;
                ifStmt->condition = optimizeExpr(ifStmt->condition, currentConstVars, loopVars);

                // ����������������Ϊ����
                if (auto num = std::dynamic_pointer_cast<NumberExpr>(ifStmt->condition)) {
                    if (num->value != 0) {
                        // ����Ϊ�棬�滻Ϊthen���
                        stmt = ifStmt->thenStmt;
                    }
                    else if (ifStmt->elseStmt) {
                        // ����Ϊ�٣��滻Ϊelse���
                        stmt = ifStmt->elseStmt;
                    }
                    else {
                        // ����Ϊ������else��ɾ������if���
                        it = block->statements.erase(it);
                        continue;
                    }
                }
            }
            else if (auto exprStmt = std::dynamic_pointer_cast<ExprStmt>(stmt)) {
                // �Ż����ʽ
                std::set<std::string> loopVars;
                exprStmt->expr = optimizeExpr(exprStmt->expr, currentConstVars, loopVars);

                // ������ʽ�ǳ�����ɾ�������
                if (std::dynamic_pointer_cast<NumberExpr>(exprStmt->expr)) {
                    it = block->statements.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }

    // �鼶�������������
    eliminateDeadCode(block);

    // �޸�: ȷ������������б�����Ч��
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

    // ���������������滻Ϊ����
    if (auto var = std::dynamic_pointer_cast<VariableExpr>(expr)) {
        auto it = constVars.find(var->name);
        if (it != constVars.end()) {
            return std::make_shared<NumberExpr>(it->second);
        }
        loopInvariants.insert(var->name);
        return var;
    }

    // ��Ԫ���ʽ�Ż�
    if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
        bin->lhs = optimizeExpr(bin->lhs, constVars, loopInvariants);
        bin->rhs = optimizeExpr(bin->rhs, constVars, loopInvariants);

        // �����۵�
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

    // ���������Ż�
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

    // �ռ�ѭ���п��ܱ��޸ĵı���
    std::set<std::string> modifiedVars;
    collectModifiedVars(whileStmt->body, modifiedVars);

    // �ռ�ѭ�������еı���
    std::set<std::string> condVars;
    collectVarsInExpr(whileStmt->condition, condVars);

    // �ϲ�����
    std::set<std::string> loopVars;
    loopVars.insert(modifiedVars.begin(), modifiedVars.end());
    loopVars.insert(condVars.begin(), condVars.end());

    // ȷ��ѭ�����ǿ����
    auto bodyBlock = std::dynamic_pointer_cast<BlockStmt>(whileStmt->body);
    if (!bodyBlock) {
        // ���ѭ���岻�ǿ���䣬����ת��Ϊ�����
        auto newBody = std::make_shared<BlockStmt>();
        newBody->statements.push_back(whileStmt->body);
        whileStmt->body = newBody;
        bodyBlock = newBody;
    }

    // ��ȡѭ������ʽ
    auto& stmts = bodyBlock->statements;
    std::vector<std::shared_ptr<Stmt>> hoistedStmts;

    for (auto it = stmts.begin(); it != stmts.end(); ) {
        bool hoist = false;

        if (auto assign = std::dynamic_pointer_cast<AssignStmt>(*it)) {
            // ��鸳ֵ�Ҳ��Ƿ���ѭ������ʽ
            if (isLoopInvariant(assign->value, loopVars)) {
                hoist = true;
            }
        }
        else if (auto decl = std::dynamic_pointer_cast<DeclareStmt>(*it)) {
            // ����ʼ�����ʽ�Ƿ���ѭ������ʽ
            if (isLoopInvariant(decl->initVal, loopVars)) {
                hoist = true;
            }
        }

        if (hoist) {
            // ��ӵ������б�
            hoistedStmts.push_back(*it);
            it = stmts.erase(it);
        }
        else {
            ++it;
        }
    }

    // �������������䣬�����µ�ѭ����
    if (!hoistedStmts.empty()) {
        // �����µ�ѭ���壨��������������ԭѭ���壩
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
        // ������ʽ����ѭ������������ѭ������ʽ
        return loopVars.find(var->name) == loopVars.end();
    }

    if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
        return isLoopInvariant(bin->lhs, loopVars) &&
            isLoopInvariant(bin->rhs, loopVars);
    }

    if (auto call = std::dynamic_pointer_cast<CallExpr>(expr)) {
        // ���躯�����ò���ѭ������ʽ�����ز��ԣ�
        return false;
    }

    return true;
}

void Optimizer::eliminateDeadCode(const std::shared_ptr<BlockStmt>& block) {
    if (!block) return;

    for (auto it = block->statements.begin(); it != block->statements.end(); ) {
        if (auto ifStmt = std::dynamic_pointer_cast<IfStmt>(*it)) {
            // �ݹ�����������
            if (auto thenBlock = std::dynamic_pointer_cast<BlockStmt>(ifStmt->thenStmt)) {
                eliminateDeadCode(thenBlock);
            }
            else if (ifStmt->thenStmt) {
                // ������ǿ���䣬ת��Ϊ�����
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
                    // ������ǿ���䣬ת��Ϊ�����
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
                // ������ǿ���䣬ת��Ϊ�����
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

    // �˷�ת��λ
    if (bin->op == "*") {
        if (auto rhsNum = std::dynamic_pointer_cast<NumberExpr>(bin->rhs)) {
            int val = rhsNum->value;
            if (val > 0 && (val & (val - 1)) == 0) { // �ж��Ƿ�Ϊ2����
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