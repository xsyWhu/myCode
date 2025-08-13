#ifndef TOKEN_H
#define TOKEN_H

#include <cstdio>

/* 不在这里定义 token 常量；token 常量由 bison 生成的 parser.tab.h 提供 */
extern int cur_line_num;
void print_token(int token);

#endif // TOKEN_H
