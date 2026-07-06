#include "Token.h"

/*
 * isKeyword 用 switch 把所有 PL/0 保留字枚举出来。
 *
 * 执行流程：
 * 1. 调用者传入某个 TokenType。
 * 2. switch 根据类别分支判断。
 * 3. 如果落在 Const/Var/.../Write 任意一个分支，就返回 true。
 * 4. 其他类型，例如 Identifier、Number、Plus，统一进入 default 返回 false。
 */
bool isKeyword(TokenType type) {
    switch (type) {
        case TokenType::Const:
        case TokenType::Var:
        case TokenType::Procedure:
        case TokenType::Call:
        case TokenType::Begin:
        case TokenType::End:
        case TokenType::If:
        case TokenType::Then:
        case TokenType::While:
        case TokenType::Do:
        case TokenType::Odd:
        case TokenType::Read:
        case TokenType::Write:
            return true;
        default:
            return false;
    }
}

/*
 * 判断是否为运算符。
 *
 * 这里把“算术运算符”“关系运算符”“赋值符号”都归到运算符分类下，
 * 主要是为了 lex 模式输出时能显示统一的中文类别。
 */
bool isOperator(TokenType type) {
    switch (type) {
        case TokenType::Plus:
        case TokenType::Minus:
        case TokenType::Times:
        case TokenType::Slash:
        case TokenType::Equal:
        case TokenType::NotEqual:
        case TokenType::Less:
        case TokenType::LessEqual:
        case TokenType::Greater:
        case TokenType::GreaterEqual:
        case TokenType::Assign:
            return true;
        default:
            return false;
    }
}

/*
 * 判断是否为界符。
 *
 * 界符本身一般不参与计算，只负责把语法成分隔开。
 * 例如：
 *   const a = 1, b = 2;
 * 逗号和分号就是声明列表中的边界标记。
 */
bool isDelimiter(TokenType type) {
    switch (type) {
        case TokenType::Comma:
        case TokenType::Semicolon:
        case TokenType::LParen:
        case TokenType::RParen:
        case TokenType::Period:
            return true;
        default:
            return false;
    }
}

/*
 * 判断一个 TokenType 能否作为 statement 的开头。
 *
 * 这个函数主要服务于 Parser::synchronize()：
 * 当语法分析遇到错误时，如果继续逐个 Token 乱读，可能产生一串连锁错误。
 * 所以程序会跳过一段 Token，直到看见“可能是下一条语句开头”的符号再继续。
 */
bool isStatementStart(TokenType type) {
    switch (type) {
        case TokenType::Identifier:
        case TokenType::Call:
        case TokenType::Begin:
        case TokenType::If:
        case TokenType::While:
        case TokenType::Read:
        case TokenType::Write:
            return true;
        default:
            return false;
    }
}

/*
 * tokenCategoryName 返回面向用户的中文分类名。
 *
 * 一行一行看它的优先级：
 * 1. 如果 token.error 非空，说明 Lexer 已经记录了具体错误，
 *    直接返回错误信息，例如“标识符长度超长”。
 * 2. 否则判断是否是保留字。
 * 3. 再判断标识符、无符号整数、运算符、界符。
 * 4. Invalid 但没有具体错误时，给出通用“非法字符(串)”。
 * 5. 理论上不会走到最后，但为了函数完整性返回“未知”。
 */
std::string tokenCategoryName(const Token& token) {
    if (!token.error.empty()) {
        return token.error;
    }
    if (isKeyword(token.type)) {
        return "保留字";
    }
    if (token.type == TokenType::Identifier) {
        return "标识符";
    }
    if (token.type == TokenType::Number) {
        return "无符号整数";
    }
    if (isOperator(token.type)) {
        return "运算符";
    }
    if (isDelimiter(token.type)) {
        return "界符";
    }
    if (token.type == TokenType::Invalid) {
        return "非法字符(串)";
    }
    return "未知";
}

/*
 * tokenTypeDebugName 返回更接近“程序内部”的英文/符号名。
 *
 * 它和 tokenCategoryName 的区别：
 * - tokenCategoryName 给用户看类别，例如“保留字”。
 * - tokenTypeDebugName 给调试和分析表看具体终结符，例如 "while"、":="、"identifier"。
 *
 * SLRParser 会把 Token 转成终结符字符串，因此这类函数对调试 LR 表很有用。
 */
std::string tokenTypeDebugName(TokenType type) {
    switch (type) {
        case TokenType::Identifier: return "identifier";
        case TokenType::Number: return "number";
        case TokenType::Const: return "const";
        case TokenType::Var: return "var";
        case TokenType::Procedure: return "procedure";
        case TokenType::Call: return "call";
        case TokenType::Begin: return "begin";
        case TokenType::End: return "end";
        case TokenType::If: return "if";
        case TokenType::Then: return "then";
        case TokenType::While: return "while";
        case TokenType::Do: return "do";
        case TokenType::Odd: return "odd";
        case TokenType::Read: return "read";
        case TokenType::Write: return "write";
        case TokenType::Plus: return "+";
        case TokenType::Minus: return "-";
        case TokenType::Times: return "*";
        case TokenType::Slash: return "/";
        case TokenType::Equal: return "=";
        case TokenType::NotEqual: return "#";
        case TokenType::Less: return "<";
        case TokenType::LessEqual: return "<=";
        case TokenType::Greater: return ">";
        case TokenType::GreaterEqual: return ">=";
        case TokenType::Assign: return ":=";
        case TokenType::Comma: return ",";
        case TokenType::Semicolon: return ";";
        case TokenType::LParen: return "(";
        case TokenType::RParen: return ")";
        case TokenType::Period: return ".";
        case TokenType::EndOfFile: return "EOF";
        case TokenType::Invalid: return "invalid";
    }
    return "unknown";
}
