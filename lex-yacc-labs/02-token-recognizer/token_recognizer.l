%option noyywrap nodefault

%{
#include <stdio.h>
%}

%%

[ \t\r\n]+      ;
[A-Za-z]+       { printf("%s 单词\n", yytext); }
[0-9]+          { printf("%s 数字\n", yytext); }
.               { printf("%s 符号\n", yytext); }

%%

int main(void)
{
    yylex();
    return 0;
}
