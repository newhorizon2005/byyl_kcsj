#pragma once

#include <string>

/*
 * TokenType 是整个编译器前端最基础的“词法类别”枚举。
 *
 * 编译器不会让语法分析器直接读取源代码字符，而是先由 Lexer 把源代码切成
 * 一个个 Token。例如：
 *
 *     const a = 10;
 *
 * 会被切成：
 *
 *     Const("const") Identifier("a") Equal("=") Number("10") Semicolon(";")
 *
 * 这里枚举值描述的是“类别”，Token.lexeme 保存的是源代码中的“原文”。
 */
enum class TokenType {
    // 普通标识符，例如变量名 x、过程名 p。PL/0 中标识符由字母开头，可跟字母或数字。
    Identifier,

    // 无符号整数，例如 0、123、999。当前项目限制长度不超过 8 位。
    Number,

    // 下面这一组是 PL/0 语言的保留字。它们看起来也是字母串，
    // 但不能当成普通变量名使用，所以 Lexer 会优先识别成专门的 TokenType。
    Const,
    Var,
    Procedure,
    Call,
    Begin,
    End,
    If,
    Then,
    While,
    Do,
    Odd,
    Read,
    Write,

    // 下面这一组是运算符。
    // Plus/Minus/Times/Slash 用于算术表达式；
    // Equal/NotEqual/Less/... 用于条件判断；
    // Assign 对应 PL/0 的赋值符号 :=，不要和数学等号 = 混淆。
    Plus,
    Minus,
    Times,
    Slash,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Assign,

    // 下面这一组是界符，也就是分隔语法结构的符号。
    // 例如逗号分隔变量声明，分号分隔语句，句点结束整个程序。
    Comma,
    Semicolon,
    LParen,
    RParen,
    Period,

    // Lexer 会在 Token 序列最后追加 EndOfFile，语法分析器用它判断输入结束。
    EndOfFile,

    // Invalid 表示词法阶段已经发现错误，例如非法字符、数字后接字母、注释未闭合等。
    Invalid
};

/*
 * Token 是词法分析器输出的一个“单词对象”。
 *
 * type   : 单词类别，例如 Identifier、Number、If。
 * lexeme : 源代码原文，例如 type 是 Identifier 时，lexeme 可能是 "count"。
 * line   : 该 Token 起始行号，用于报错定位。
 * column : 该 Token 起始列号，用于更细的调试定位。
 * error  : 如果这个 Token 本身有词法错误，就把中文错误信息放在这里。
 *
 * 注意：为了让错误输出还能继续显示原始文本，出错时也会保留 lexeme。
 */
struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
    std::string error;
};

// 判断一个 TokenType 是否属于保留字类别。
bool isKeyword(TokenType type);

// 判断一个 TokenType 是否属于运算符类别。
bool isOperator(TokenType type);

// 判断一个 TokenType 是否属于界符类别。
bool isDelimiter(TokenType type);

/*
 * 判断某个 Token 是否可能作为“语句”的开头。
 *
 * 递归下降语法分析中的 statement 可以有很多分支：
 *   标识符 := 表达式
 *   call 标识符
 *   begin ... end
 *   if ... then ...
 *   while ... do ...
 *   read(...)
 *   write(...)
 *
 * synchronize() 错误恢复时会用这个函数寻找下一个像语句开头的位置。
 */
bool isStatementStart(TokenType type);

// 把 Token 转成面向课程验收的中文分类名，例如“保留字”“标识符”“运算符”。
std::string tokenCategoryName(const Token& token);

// 把 TokenType 转成调试用字符串，主要用于 LR 分析表、错误信息和内部输出。
std::string tokenTypeDebugName(TokenType type);
