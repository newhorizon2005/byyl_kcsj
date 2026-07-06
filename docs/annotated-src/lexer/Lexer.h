#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "../common/Token.h"

/*
 * Lexer 是词法分析器。
 *
 * 它的输入是一整段源代码字符串 source_，
 * 输出是 std::vector<Token>，也就是 Token 序列。
 *
 * 可以把它想成一个带指针的扫描器：
 *
 *     source_: const a = 10;
 *              ^
 *            pos_
 *
 * 每次识别一个完整单词后，pos_ 会向后移动。
 * line_ 和 column_ 同步更新，用于错误定位。
 */
class Lexer {
public:
    // 构造函数接收源代码全文，并初始化保留字表 keywords_。
    explicit Lexer(std::string source);

    // 对外唯一主要接口：扫描完整 source_，返回 Token 列表。
    std::vector<Token> tokenize();

private:
    // 查看当前位置之后 offset 个字符，但不移动 pos_。
    char peek(std::size_t offset = 0) const;

    // 消费当前位置字符：返回该字符，并把 pos_/line_/column_ 推进到下一个位置。
    char advance();

    // 判断 pos_ 是否已经走到 source_ 末尾。
    bool isAtEnd() const;

    // 如果当前位置字符等于 expected，就消费它并返回 true；否则不动并返回 false。
    bool match(char expected);

    // 跳过空白符和注释。多行注释未闭合时，会往 tokens 中放入一个错误 Token。
    void skipWhitespaceAndComments(std::vector<Token>& tokens);

    // 从当前位置开始扫描“标识符或保留字”。
    Token scanIdentifier();

    // 从当前位置开始扫描“无符号整数”，并检查数字后接字母、整数过长等错误。
    Token scanNumber();

    // 统一构造 Token，避免每个分支重复写 Token{...}。
    Token makeToken(TokenType type, const std::string& lexeme, int line, int column, const std::string& error = "") const;

    // 源代码全文。
    std::string source_;

    // 当前扫描位置，是 source_ 的下标。
    std::size_t pos_ = 0;

    // 当前行号，从 1 开始。
    int line_ = 1;

    // 当前列号，从 1 开始。
    int column_ = 1;

    // 保留字表：把 "while" 这类字符串映射到 TokenType::While。
    std::unordered_map<std::string, TokenType> keywords_;
};
