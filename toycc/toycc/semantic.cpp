#include "semantic.h"
#include "ast.h"  


void SemanticAnalyzer::enterScope() {
    varScopes.push({});
}

void SemanticAnalyzer::exitScope() {
    varScopes.pop();
}

void SemanticAnalyzer::declareVar(const std::string& name, const std::string& type) {
    if (varScopes.top().count(name)) {
        throw std::runtime_error("�����ظ�����: " + name);
    }
    varScopes.top()[name] = VarInfo{ type, true };
}

bool SemanticAnalyzer::isVarDeclared(const std::string& name) {
    auto scopes = varScopes;
    while (!scopes.empty()) {
        auto& table = scopes.top();
        if (table.count(name)) return true;
        scopes.pop();
    }
    return false;
}

void SemanticAnalyzer::analyze(const std::vector<std::shared_ptr<FuncDef>>& funcs) {
    bool hasMain = false;

    for (const auto& func : funcs) {
        if (funcTable.count(func->name)) {
            throw std::runtime_error("�����ظ�����: " + func->name);
        }
        funcTable[func->name] = func->retType;

        if (func->name == "main") {
            if (func->retType != "int" || !func->params.empty()) {
                throw std::runtime_error("main �������뷵�� int ���޲���");
            }
            hasMain = true;
        }
    }

    if (!hasMain) {
        throw std::runtime_error("ȱ�� main ����");
    }

    for (const auto& func : funcs) {
        checkFunc(func);
    }
}

void SemanticAnalyzer::checkFunc(const std::shared_ptr<FuncDef>& func) {
    currentFuncRetType = func->retType;
    enterScope();

    for (const auto& param : func->params) {
        declareVar(param.name, "int");
    }

    checkStmt(func->body);
    exitScope();
}

void SemanticAnalyzer::checkStmt(const std::shared_ptr<Stmt>& stmt) {
    if (auto block = std::dynamic_pointer_cast<BlockStmt>(stmt)) {
        enterScope();
        for (auto& s : block->statements) {
            checkStmt(s);
        }
        exitScope();
    }
    else if (auto ret = std::dynamic_pointer_cast<ReturnStmt>(stmt)) {
        if (currentFuncRetType == "int" && !ret->value) {
            throw std::runtime_error("int �������뷵��ֵ");
        }
        if (currentFuncRetType == "void" && ret->value) {
            throw std::runtime_error("void �������ܷ���ֵ");
        }
        if (ret->value) checkExpr(ret->value);
    }
    else if (auto decl = std::dynamic_pointer_cast<DeclareStmt>(stmt)) {
        checkExpr(decl->initVal);
        declareVar(decl->varName, "int");
    }
    else if (auto assign = std::dynamic_pointer_cast<AssignStmt>(stmt)) {
        if (!isVarDeclared(assign->varName)) {
            throw std::runtime_error("����δ����: " + assign->varName);
        }
        checkExpr(assign->value);
    }
    else if (auto exprStmt = std::dynamic_pointer_cast<ExprStmt>(stmt)) {
        if (exprStmt->expr) checkExpr(exprStmt->expr);
    }
    else if (auto ifStmt = std::dynamic_pointer_cast<IfStmt>(stmt)) {
        checkExpr(ifStmt->condition);
        checkStmt(ifStmt->thenStmt);
        if (ifStmt->elseStmt) checkStmt(ifStmt->elseStmt);
    }
    else if (auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(stmt)) {
        checkExpr(whileStmt->condition);
        bool old = inLoop;
        inLoop = true;
        checkStmt(whileStmt->body);
        inLoop = old;
    }
    else if (dynamic_cast<BreakStmt*>(stmt.get())) {
        // break语句，语义分析通过
    }
    else if (dynamic_cast<ContinueStmt*>(stmt.get())) {
        // continue语句，语义分析通过
    }
}

void SemanticAnalyzer::checkExpr(const std::shared_ptr<Expr>& expr) {
    if (auto var = std::dynamic_pointer_cast<VariableExpr>(expr)) {
        if (!isVarDeclared(var->name)) {
            throw std::runtime_error("����δ����: " + var->name);
        }
    }
    else if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
        if (bin->lhs) checkExpr(bin->lhs);
        if (bin->rhs) checkExpr(bin->rhs);
    }
    else if (auto call = std::dynamic_pointer_cast<CallExpr>(expr)) {
        if (!funcTable.count(call->callee)) {
            throw std::runtime_error("����δ���庯��: " + call->callee);
        }
        for (auto& arg : call->args) {
            checkExpr(arg);
        }
    }
    else if (std::dynamic_pointer_cast<NumberExpr>(expr)) {
        // �������Ϸ�
    }
}
