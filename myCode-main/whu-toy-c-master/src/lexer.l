%{
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include "parser.tab.h"
#include "token.h"

int cur_line_num = 1;
%}

%x COMMENT

%option noyywrap

%%


[ \t\r\n]+ {
    for (unsigned char *p = (unsigned char*)yytext; *p; ++p)
        if (*p == '\n') ++cur_line_num;
}


"//".* {
    for (unsigned char *p = (unsigned char*)yytext; *p; ++p)
        if (*p == '\n') ++cur_line_num;
}


"/*"  { BEGIN(COMMENT); }


<COMMENT>{
    "*/"      { BEGIN(INITIAL); }
    \n        { ++cur_line_num; }
    .         { /* skip other comment chars */ }
    <<EOF>>   { fprintf(stderr, "Unterminated comment at line %d\n", cur_line_num); exit(1); }
}


"||"    { return T_Or; }
"&&"    { return T_And; }
"<="    { return T_Le; }
">="    { return T_Ge; }
"=="    { return T_Eq; }
"!="    { return T_Ne; }


"void"      { return T_Void; }
"int"       { return T_Int; }
"while"     { return T_While; }
"if"        { return T_If; }
"else"      { return T_Else; }
"return"    { return T_Return; }
"break"     { return T_Break; }
"continue"  { return T_Continue; }


(0|[1-9][0-9]*) {
    yylval.str_val = new std::string(yytext);
    return T_IntConstant;
}


[A-Za-z_][A-Za-z0-9_]* {
    yylval.str_val = new std::string(yytext);
    return T_Identifier;
}


[+\-*/%=\;\(\),\<\>\{\}\!\[\]] { return yytext[0]; }


. {
    fprintf(stderr, "Lexical error at line %d: Unrecognized character '%s'\n", cur_line_num, yytext);
}

<<EOF>> { return 0; }

%%
