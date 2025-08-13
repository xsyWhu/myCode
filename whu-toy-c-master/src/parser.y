%{
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <string>
#include "token.h"
#include "ast.h"

/* 外部符号 */
extern int yylex();
extern int cur_line_num;
extern char* yytext;

/* 错误处理 */
void yyerror(CompUnit** root, const char* msg);

/* 工厂函数（接受 C++ 类型） */
static Expr* make_int_const(int v) {
    Expr* e = new Expr();
    e->type = Expr::INT_CONST;
    e->int_val = v;
    return e;
}
static Expr* make_identifier(const std::string& name) {
    Expr* e = new Expr();
    e->type = Expr::IDENTIFIER;
    e->id_name = name;
    return e;
}
static Expr* make_unary(Expr* child, char op) {
    Expr* e = new Expr();
    e->type = Expr::UNARY_OP;
    e->child = child;
    e->op_char = op;
    return e;
}
static Expr* make_binary(Expr* l, Expr* r, const std::string& op) {
    Expr* e = new Expr();
    e->type = Expr::BINARY_OP;
    e->left = l;
    e->right = r;
    e->op_str = op;
    return e;
}
static Expr* make_call(const std::string& name, const std::vector<Expr*>& args) {
    Expr* e = new Expr();
    e->type = Expr::FUNC_CALL;
    e->call_name = name;
    e->args = args; // copy pointers
    return e;
}

/* Stmt 工厂 */
static Stmt* make_block(const std::vector<Stmt*>& stmts) {
    Stmt* s = new Stmt();
    s->type = Stmt::BLOCK;
    s->block_stmts = stmts;
    return s;
}
static Stmt* make_empty() {
    Stmt* s = new Stmt();
    s->type = Stmt::EMPTY;
    return s;
}
static Stmt* make_expr_stmt(Expr* e) {
    Stmt* s = new Stmt();
    s->type = Stmt::EXPR;
    s->expr_stmt = e;
    return s;
}
static Stmt* make_assign(const std::string& id, Expr* rhs) {
    Stmt* s = new Stmt();
    s->type = Stmt::ASSIGN;
    s->assign_id = id;
    s->assign_rhs = rhs;
    return s;
}
static Stmt* make_declare(const std::string& id, Expr* init) {
    Stmt* s = new Stmt();
    s->type = Stmt::DECLARE;
    s->declare_id = id;
    s->declare_init = init;
    return s;
}
static Stmt* make_if(Expr* cond, Stmt* then_s, Stmt* else_s) {
    Stmt* s = new Stmt();
    s->type = Stmt::IF;
    s->if_cond = cond;
    s->if_then = then_s;
    s->if_else = else_s;
    return s;
}
static Stmt* make_while(Expr* cond, Stmt* body) {
    Stmt* s = new Stmt();
    s->type = Stmt::WHILE;
    s->while_cond = cond;
    s->while_body = body;
    return s;
}
static Stmt* make_break() {
    Stmt* s = new Stmt();
    s->type = Stmt::BREAK;
    return s;
}
static Stmt* make_continue() {
    Stmt* s = new Stmt();
    s->type = Stmt::CONTINUE;
    return s;
}
static Stmt* make_return(Expr* ret) {
    Stmt* s = new Stmt();
    s->type = Stmt::RETURN;
    s->return_expr = ret;
    return s;
}

/* FuncDef 构造 */
static FuncDef* make_func(const std::string& ret_type, const std::string& name, const std::vector<std::string>& params, Stmt* body) {
    FuncDef* f = new FuncDef();
    f->return_type = ret_type;
    f->name = name;
    f->params = params;
    f->body = body;
    return f;
}

%}

/* 确保 parser.tab.h 能看到这些类型 */
%code requires {
    #include <vector>
    #include <string>
    #include "ast.h"
}

/* parse 参数（把解析后的 AST root 传回调用者） */
%parse-param { CompUnit** root }

/* tokens（语义值用 std::string*） */
%token T_Le T_Ge T_Eq T_Ne T_And T_Or
%token T_Void T_Int T_While T_If T_Else T_Return T_Break T_Continue
%token <str_val> T_Identifier
%token <str_val> T_IntConstant

/* 优先级设置（为 dangling else） */
%nonassoc LOWER_THAN_ELSE
%nonassoc T_Else

/* 属性联合体（用指针持有堆对象以便在动作中管理） */
%union {
    std::string* str_val;
    Expr* expr;
    Stmt* stmt;
    FuncDef* func;
    CompUnit* comp_unit;
    std::vector<std::string>* param_list;
    std::vector<Stmt*>* stmt_list;
    std::vector<Expr*>* expr_list;
}

/* 非终结符类型绑定 */
%type <expr> Expr LOrExpr LAndExpr RelExpr AddExpr MulExpr UnaryExpr PrimaryExpr
%type <stmt> Stmt Block
%type <func> FuncDef
%type <comp_unit> CompUnit
%type <param_list> ParamList Param
%type <stmt_list> StmtList
%type <expr_list> ArgList

%%

/* 1. CompUnit */
CompUnit:
    FuncDef {
        $$ = new CompUnit;
        $$->funcs.push_back($1);
        *root = $$;
    }
    | CompUnit FuncDef {
        $1->funcs.push_back($2);
        $$ = $1;
    }
    ;

/* 2. FuncDef */
FuncDef:
    T_Int T_Identifier '(' ParamList ')' Block {
        std::vector<std::string> params = *$4;
        delete $4;
        std::string name = *$2; delete $2;
        $$ = make_func("int", name, params, $6);
    }
    | T_Void T_Identifier '(' ParamList ')' Block {
        std::vector<std::string> params = *$4;
        delete $4;
        std::string name = *$2; delete $2;
        $$ = make_func("void", name, params, $6);
    }
    ;

/* 3. ParamList */
ParamList:
    /* empty */ { $$ = new std::vector<std::string>; }
    | Param { $$ = $1; }
    ;

Param:
    T_Int T_Identifier {
        $$ = new std::vector<std::string>;
        $$->push_back(*$2);
        delete $2;
    }
    | Param ',' T_Int T_Identifier {
        $1->push_back(*$4);
        delete $4;
        $$ = $1;
    }
    ;

/* 4. Stmt */
Stmt:
    Block { $$ = $1; }
    | ';' { $$ = make_empty(); }
    | Expr ';' { $$ = make_expr_stmt($1); }
    | T_Identifier '=' Expr ';' {
        std::string id = *$1; delete $1;
        $$ = make_assign(id, $3);
    }
    | T_Int T_Identifier '=' Expr ';' {
        std::string id = *$2; delete $2;
        $$ = make_declare(id, $4);
    }
    | T_If '(' Expr ')' Stmt %prec LOWER_THAN_ELSE { $$ = make_if($3, $5, nullptr); }
    | T_If '(' Expr ')' Stmt T_Else Stmt { $$ = make_if($3, $5, $7); }
    | T_While '(' Expr ')' Stmt {
        $$ = make_while($3, $5);
    }
    | T_Break ';' { $$ = make_break(); }
    | T_Continue ';' { $$ = make_continue(); }
    | T_Return ';' { $$ = make_return(nullptr); }
    | T_Return Expr ';' { $$ = make_return($2); }
    ;

/* 5. Block */
Block:
    '{' StmtList '}' {
        std::vector<Stmt*> stms = *$2;
        delete $2;
        $$ = make_block(stms);
    }
    ;

/* 6. StmtList */
StmtList:
    /* empty */ { $$ = new std::vector<Stmt*>; }
    | StmtList Stmt {
        $1->push_back($2);
        $$ = $1;
    }
    ;

/* 7. Expr -> LOrExpr */
Expr:
    LOrExpr { $$ = $1; }
    ;

/* 8. LOrExpr */
LOrExpr:
    LAndExpr { $$ = $1; }
    | LOrExpr T_Or LAndExpr { $$ = make_binary($1, $3, "||"); }
    ;

/* 9. LAndExpr */
LAndExpr:
    RelExpr { $$ = $1; }
    | LAndExpr T_And RelExpr { $$ = make_binary($1, $3, "&&"); }
    ;

/* 10. RelExpr */
RelExpr:
    AddExpr { $$ = $1; }
    | RelExpr '<' AddExpr { $$ = make_binary($1, $3, "<"); }
    | RelExpr '>' AddExpr { $$ = make_binary($1, $3, ">"); }
    | RelExpr T_Le AddExpr { $$ = make_binary($1, $3, "<="); }
    | RelExpr T_Ge AddExpr { $$ = make_binary($1, $3, ">="); }
    | RelExpr T_Eq AddExpr { $$ = make_binary($1, $3, "=="); }
    | RelExpr T_Ne AddExpr { $$ = make_binary($1, $3, "!="); }
    ;

/* 11. AddExpr */
AddExpr:
    MulExpr { $$ = $1; }
    | AddExpr '+' MulExpr { $$ = make_binary($1, $3, "+"); }
    | AddExpr '-' MulExpr { $$ = make_binary($1, $3, "-"); }
    ;

/* 12. MulExpr */
MulExpr:
    UnaryExpr { $$ = $1; }
    | MulExpr '*' UnaryExpr { $$ = make_binary($1, $3, "*"); }
    | MulExpr '/' UnaryExpr { $$ = make_binary($1, $3, "/"); }
    | MulExpr '%' UnaryExpr { $$ = make_binary($1, $3, "%"); }
    ;

/* 13. UnaryExpr */
UnaryExpr:
    PrimaryExpr { $$ = $1; }
    | '+' UnaryExpr { $$ = make_unary($2, '+'); }
    | '-' UnaryExpr { $$ = make_unary($2, '-'); }
    | '!' UnaryExpr { $$ = make_unary($2, '!'); }
    ;

/* 14. PrimaryExpr (函数调用改为任意参数) */
PrimaryExpr:
    T_Identifier {
        std::string id = *$1; delete $1;
        $$ = make_identifier(id);
    }
    | T_IntConstant {
        int v = 0;
        try { v = std::stoi(*$1); } catch(...) { v = 0; }
        delete $1;
        $$ = make_int_const(v);
    }
    | '(' Expr ')' { $$ = $2; }
    | T_Identifier '(' ArgList ')' {
        std::string name = *$1; delete $1;
        std::vector<Expr*> args = *$3; delete $3;
        $$ = make_call(name, args);
    }
    ;

/* ArgList 支持任意参数 */
ArgList:
    /* empty */ { $$ = new std::vector<Expr*>; }
    | Expr { $$ = new std::vector<Expr*>; $$->push_back($1); }
    | ArgList ',' Expr { $1->push_back($3); $$ = $1; }
    ;

%%

/* 错误处理 */
void yyerror(CompUnit** root, const char* msg) {
    fprintf(stderr, "Syntax error at line %d: %s (near '%s')\n", cur_line_num, msg ? msg : "unknown", yytext ? yytext : "");
}


