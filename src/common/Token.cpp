#include "Token.h"

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
