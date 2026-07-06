%option noyywrap nodefault

%{
#include <ctype.h>
#include <stdio.h>

static unsigned long counts[26];
static unsigned long total_letters = 0;

static void print_frequencies(void)
{
    int i;

    if (total_letters == 0) {
        printf("No letters found.\n");
        return;
    }

    int first = 1;
    for (i = 0; i < 26; ++i) {
        if (counts[i] > 0) {
            double percent = (double)counts[i] * 100.0 / (double)total_letters;
            if (!first) {
                printf(", ");
            }
            printf("%c:%.0f%%", 'A' + i, percent);
            first = 0;
        }
    }
    printf("\n");
}
%}

%%

[A-Za-z] {
    unsigned char ch = (unsigned char)yytext[0];
    int index = toupper(ch) - 'A';
    counts[index]++;
    total_letters++;
}

.|\n    ;

<<EOF>> {
    print_frequencies();
    return 0;
}

%%

int main(void)
{
    yylex();
    return 0;
}
