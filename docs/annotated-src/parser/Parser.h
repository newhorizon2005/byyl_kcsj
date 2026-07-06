#pragma once

#include <set>
#include <string>
#include <vector>

#include "../common/Token.h"
#include "../semantic/Semantic.h"

/*
 * SyntaxError 表示一个语法错误。
 *
 * line    : 错误所在行。
 * message : 中文错误说明。
 *
 * Parser 不会一遇到错误就立刻退出，而是尽量继续向后分析，
 * 因此 errors 是一个数组。
 */
struct SyntaxError {
    int line;
    std::string message;
};

/*
 * ConditionValue 是“条件表达式”的中间表示。
 *
 * 条件有两种形式：
 *   odd Expression
 *   Expression RelOp Expression
 *
 * 例如：
 *   odd x       -> op = "odd", left = "x", right = "_"
 *   a < b       -> op = "<",   left = "a", right = "b"
 *
 * Parser 在 parseCondition() 中只把条件拆出来，不直接判断真假。
 * if/while 会拿这个结构去生成条件跳转四元式。
 */
struct ConditionValue {
    std::string op;
    std::string left;
    std::string right;
};

/*
 * Parser 是手写递归下降语法分析器。
 *
 * “递归下降”的核心思想：
 * - 每个非终结符写成一个函数。
 * - 函数内部根据当前 Token 决定匹配哪条产生式。
 * - 当某个产生式里包含另一个非终结符时，就调用对应函数。
 *
 * 本项目实现的 PL/0 文法可以按下面理解：
 *
 *   Program       -> Block "."
 *   Block         -> [ConstDecl] [VarDecl] { ProcedureDecl } Statement
 *
 *   ConstDecl     -> "const" ident "=" number { "," ident "=" number } ";"
 *   VarDecl       -> "var" ident { "," ident } ";"
 *   ProcedureDecl -> "procedure" ident ";" Block ";"
 *
 *   Statement     -> ident ":=" Expression
 *                  | "call" ident
 *                  | "begin" Statement { ";" Statement } "end"
 *                  | "if" Condition "then" Statement
 *                  | "while" Condition "do" Statement
 *                  | "read" "(" ident { "," ident } ")"
 *                  | "write" "(" Expression { "," Expression } ")"
 *                  | epsilon
 *
 *   Condition     -> "odd" Expression
 *                  | Expression RelOp Expression
 *
 *   RelOp         -> "=" | "#" | "<" | "<=" | ">" | ">="
 *   Expression    -> [ "+" | "-" ] Term { ( "+" | "-" ) Term }
 *   Term          -> Factor { ( "*" | "/" ) Factor }
 *   Factor        -> ident | number | "(" Expression ")"
 *
 * 函数对应关系：
 * - Program       -> parseProgram()
 * - Block         -> parseBlock()
 * - ConstDecl     -> parseConstDecl()
 * - VarDecl       -> parseVarDecl()
 * - ProcedureDecl -> parseProcedureDecl()
 * - Statement     -> parseStatement()
 * - Condition     -> parseCondition()
 * - Expression    -> parseExpression()
 * - Term          -> parseTerm()
 * - Factor        -> parseFactor()
 */
class Parser {
public:
    /*
     * tokens 是 Lexer 输出的 Token 序列。
     *
     * semantic 可以为空：
     * - semantic == nullptr：只做语法分析。
     * - semantic != nullptr：语法分析的同时做语义检查、生成四元式。
     */
    Parser(std::vector<Token> tokens, SemanticAnalyzer* semantic = nullptr);

    // 分析完整程序，成功返回 true，失败返回 false。
    bool parseProgram();

    // 获取语法错误列表，供 main.cpp 打印。
    const std::vector<SyntaxError>& syntaxErrors() const;

private:
    // 当前正在看的 Token，也就是 tokens_[pos_]。
    const Token& current() const;

    // 上一个已经消费过的 Token。
    const Token& previous() const;

    // 是否已经到 EOF。
    bool isAtEnd() const;

    // 当前 Token 是否是指定类型；只看不消费。
    bool check(TokenType type) const;

    // 如果当前 Token 是指定类型，就消费并返回 true；否则不动并返回 false。
    bool match(TokenType type);

    // 消费当前 Token，并返回刚刚消费的 Token。
    Token advance();

    // 强制要求当前 Token 是指定类型；不是则报错，但返回一个占位 Token 继续分析。
    Token expect(TokenType type, const std::string& message);

    // 记录语法错误。
    void addSyntaxError(int line, const std::string& message);

    // 错误恢复：跳到下一个较可能继续分析的位置。当前代码保留了该工具函数。
    void synchronize();

    // 下面这些函数与文法非终结符一一对应。
    // 阅读顺序建议按 Program -> Block -> Statement -> Expression 往下看。
    void parseBlock();
    void parseConstDecl();
    void parseVarDecl();
    void parseProcedureDecl();

    // 语句层：根据当前 token 选择赋值、call、begin、if、while、read、write 或空语句。
    void parseStatement();

    // 条件层：解析 odd 表达式或左右表达式加关系运算符。
    ConditionValue parseCondition();

    // 表达式层：Expression 处理加减和一元正负号，Term 处理乘除，Factor 处理最小因子。
    std::string parseExpression();
    std::string parseTerm();
    std::string parseFactor();

    // 把关系 Token 转成四元式中使用的关系字符串。
    std::string relationLexeme(TokenType type) const;

    // 取关系的反关系，用于生成“条件为假时跳转”的四元式。
    std::string inverseRelation(const std::string& op) const;

    // 判断当前 Token 是否是关系运算符。
    bool isRelation(TokenType type) const;

    // 把四元式编号格式化成跳转目标，例如 8 -> "$8"。
    std::string quadTarget(int index) const;

    // 待分析的 Token 序列。
    std::vector<Token> tokens_;

    // 当前 Token 下标。
    std::size_t pos_ = 0;

    // 语法错误列表。
    std::vector<SyntaxError> syntaxErrors_;

    // 记录出现过错误的行。当前主要用于保留错误管理能力。
    std::set<int> syntaxErrorLines_;

    // 可选语义分析器指针。为空时 Parser 只管语法，不生成符号表和四元式。
    SemanticAnalyzer* semantic_;
};
