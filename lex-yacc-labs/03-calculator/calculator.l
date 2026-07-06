%option noyywrap nodefault

%{
#include <stdlib.h>
#include "calculator.tab.h"
%}

%%

[0-9]+          {
                    yylval.ival = atoi(yytext);
                    return NUMBER;
                }
[ \t\r]+        ;
\n              { return '\n'; }
"+"             { return '+'; }
"*"             { return '*'; }
.               { return yytext[0]; }

%%
