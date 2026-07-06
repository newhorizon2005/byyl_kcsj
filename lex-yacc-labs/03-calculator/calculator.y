%{
#include <stdio.h>

int yylex(void);
void yyerror(const char *message);
%}

%union {
    int ival;
}

%token <ival> NUMBER
%type <ival> expr term factor

%%

input:
      lines final
    ;

lines:
      /* empty */
    | lines '\n'
    | lines expr '\n'     { printf("%d\n", $2); }
    ;

final:
      /* empty */
    | expr                { printf("%d\n", $1); }
    ;

expr:
      expr '+' term       { $$ = $1 + $3; }
    | term                { $$ = $1; }
    ;

term:
      term '*' factor     { $$ = $1 * $3; }
    | factor              { $$ = $1; }
    ;

factor:
      NUMBER              { $$ = $1; }
    ;

%%

void yyerror(const char *message)
{
    fprintf(stderr, "parse error: %s\n", message);
}

int main(void)
{
    return yyparse();
}
