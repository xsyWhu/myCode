#include "codegen.h"
#include <sstream>
#include <stdexcept>

CodeGen::CodeGen(std::ostream& out) : out(out) {}

void CodeGen::emit(const std::string& line) {
    out << "    " << line << "\n";
}

std::string CodeGen::newLabel(const std::string& base) {
    return base + "_" + std::to_string(labelCount++);
}

void CodeGen::resetStack() {
    varOffsets.clear();
    stackOffset = 0;
    breakLabels.clear();
    continueLabels.clear();
}

void CodeGen::allocVar(const std::string& name) {
    stackOffset -= 4;
    varOffsets[name] = stackOffset;
}

void CodeGen::generate(const std::vector<std::shared_ptr<FuncDef>>& funcs) {
    emit(".text");

    // 首先找到main函数并生成
    std::shared_ptr<FuncDef> mainFunc = nullptr;
    for (const auto& func : funcs) {
        if (func->name == "main") {
            mainFunc = func;
            break;
        }
    }

    if (!mainFunc) {
        throw std::runtime_error("main function not found");
    }
    genFunc(mainFunc);

    // 生成其他函数
    for (const auto& func : funcs) {
        if (func->name != "main") {
            genFunc(func);
        }
    }
}

void CodeGen::genFunc(const std::shared_ptr<FuncDef>& func) {
    resetStack();
    if (func->name == "main") {
        emit(".globl main");
    }
    emit(func->name + ":");
    emit("addi sp, sp, -256");

    for (const auto& param : func->params) {
        allocVar(param.name);
    }

    genBlock(func->body);

    emit("addi sp, sp, 256");
    emit("ret");
}

void CodeGen::genBlock(const std::shared_ptr<BlockStmt>& block) {
    for (const auto& stmt : block->statements) {
        genStmt(stmt);
    }
}

void CodeGen::genStmt(const std::shared_ptr<Stmt>& stmt) {
    if (!stmt) return;

    if (auto decl = std::dynamic_pointer_cast<DeclareStmt>(stmt)) {
        allocVar(decl->varName);
        std::string reg = genExprToReg(decl->initVal);
        emit("sw " + reg + ", " + std::to_string(varOffsets[decl->varName]) + "(sp)");
    }
    else if (auto assign = std::dynamic_pointer_cast<AssignStmt>(stmt)) {
        std::string reg = genExprToReg(assign->value);
        emit("sw " + reg + ", " + std::to_string(varOffsets[assign->varName]) + "(sp)");
    }
    else if (auto ret = std::dynamic_pointer_cast<ReturnStmt>(stmt)) {
        if (ret->value) {
            std::string reg = genExprToReg(ret->value);
            emit("mv a0, " + reg);
        }
    }
    else if (auto block = std::dynamic_pointer_cast<BlockStmt>(stmt)) {
        genBlock(block);
    }
    else if (auto exprstmt = std::dynamic_pointer_cast<ExprStmt>(stmt)) {
        genExprToReg(exprstmt->expr);
    }
    else if (auto ifstmt = std::dynamic_pointer_cast<IfStmt>(stmt)) {
        std::string cond = genExprToReg(ifstmt->condition);
        std::string l_else = newLabel("else");
        std::string l_end = newLabel("endif");
        emit("beqz " + cond + ", " + l_else);
        genStmt(ifstmt->thenStmt);
        emit("j " + l_end);
        emit(l_else + ":");
        if (ifstmt->elseStmt) {
            genStmt(ifstmt->elseStmt);
        }
        emit(l_end + ":");
    }
    else if (auto whilestmt = std::dynamic_pointer_cast<WhileStmt>(stmt)) {
        std::string l_begin = newLabel("loop");
        std::string l_end = newLabel("endloop");

        // 确保标签压栈在生成代码前
        continueLabels.push_back(l_begin);
        breakLabels.push_back(l_end);

        emit(l_begin + ":");
        std::string cond = genExprToReg(whilestmt->condition);
        emit("beqz " + cond + ", " + l_end);
        genStmt(whilestmt->body);
        emit("j " + l_begin);
        emit(l_end + ":");

        // 确保标签在循环结束后弹出
        continueLabels.pop_back();
        breakLabels.pop_back();
    }
    else if (auto brk = std::dynamic_pointer_cast<BreakStmt>(stmt)) {
        if (breakLabels.empty()) throw std::runtime_error("break outside loop");
        emit("j " + breakLabels.back());
    }
    else if (auto cont = std::dynamic_pointer_cast<ContinueStmt>(stmt)) {
        if (continueLabels.empty()) throw std::runtime_error("continue outside loop");
        emit("j " + continueLabels.back());
    }
    else {
        throw std::runtime_error("Unknown statement");
    }
}

std::string CodeGen::genExprToReg(const std::shared_ptr<Expr>& expr) {
    static int regCount = 0;
    std::string reg = "t" + std::to_string(regCount++ % 7);
    genExpr(expr, reg);
    return reg;
}

void CodeGen::genExpr(const std::shared_ptr<Expr>& expr, const std::string& dst) {
    if (auto num = std::dynamic_pointer_cast<NumberExpr>(expr)) {
        emit("li " + dst + ", " + std::to_string(num->value));
    }
    else if (auto var = std::dynamic_pointer_cast<VariableExpr>(expr)) {
        emit("lw " + dst + ", " + std::to_string(varOffsets[var->name]) + "(sp)");
    }
    else if (auto call = std::dynamic_pointer_cast<CallExpr>(expr)) {
        for (size_t i = 0; i < call->args.size(); ++i) {
            std::string argReg = genExprToReg(call->args[i]);
            emit("mv a" + std::to_string(i) + ", " + argReg);
        }
        emit("call " + call->callee);
        emit("mv " + dst + ", a0");
    }
    else if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
        std::string lhs = genExprToReg(bin->lhs);
        std::string rhs = genExprToReg(bin->rhs);
        std::string op = bin->op;

        if (op == "+") emit("add " + dst + ", " + lhs + ", " + rhs);
        else if (op == "-") emit("sub " + dst + ", " + lhs + ", " + rhs);
        else if (op == "*") emit("mul " + dst + ", " + lhs + ", " + rhs);
        else if (op == "/") emit("div " + dst + ", " + lhs + ", " + rhs);
        else if (op == "%") emit("rem " + dst + ", " + lhs + ", " + rhs);
        else if (op == "<") emit("slt " + dst + ", " + lhs + ", " + rhs);
        else if (op == ">") emit("slt " + dst + ", " + rhs + ", " + lhs);
        else if (op == "==") {
            emit("sub " + dst + ", " + lhs + ", " + rhs);
            emit("seqz " + dst + ", " + dst);
        }
        else if (op == "!=") {
            emit("sub " + dst + ", " + lhs + ", " + rhs);
            emit("snez " + dst + ", " + dst);
        }
        else if (op == "<=") {
            emit("slt " + dst + ", " + rhs + ", " + lhs);
            emit("xori " + dst + ", " + dst + ", 1");
        }
        else if (op == ">=") {
            emit("slt " + dst + ", " + lhs + ", " + rhs);
            emit("xori " + dst + ", " + dst + ", 1");
        }
        else {
            throw std::runtime_error("Unsupported binary operator: " + op);
        }
    }
    else {
        throw std::runtime_error("Unsupported expression type");
    }
}