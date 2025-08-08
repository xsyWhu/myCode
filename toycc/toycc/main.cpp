#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "codegen.h"
#include "optimizer.h"  // 确保包含优化器头文件
#include <fstream>
#include <sstream>
#include <iostream>

int main(int argc, char* argv[]) {
    std::string filePath;
    std::string outputPath = "output.s";  // 默认输出文件

    if (argc < 2) {
        filePath = "test1.tc";
        std::cout << "[INFO] No input file specified. Using default: " << filePath << std::endl;
    }
    else {
        filePath = argv[1];
        std::cout << "[INFO] Using input file: " << filePath << std::endl;
    }

    std::ifstream fin(filePath);
    if (!fin) {
        std::cerr << "[ERROR] Cannot open file: " << filePath << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << fin.rdbuf();
    std::string sourceCode = buffer.str();

    try {
        Lexer lexer(sourceCode);
        Parser parser(lexer);
        auto ast = parser.parseCompUnit();

        SemanticAnalyzer semanticAnalyzer;
        semanticAnalyzer.analyze(ast);

        // 应用优化器
        Optimizer optimizer;
        optimizer.optimize(ast);

        std::ofstream fout(outputPath);
        if (!fout) {
            std::cerr << "[ERROR] Cannot open output file: " << outputPath << std::endl;
            return 1;
        }

        CodeGen codegen(fout);
        codegen.generate(ast);
        fout.close();

        std::cout << "[SUCCESS] RISC-V assembly generated: " << outputPath << std::endl;
        return 0;

    }
    catch (const std::exception& ex) {
        std::cerr << "[FAILURE] Compilation failed: " << ex.what() << std::endl;
        return 1;
    }
}