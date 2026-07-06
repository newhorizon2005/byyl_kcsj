#pragma once

#include <string>

enum class TokenType {
    Identifier,
    Number,

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

    Comma,
    Semicolon,
    LParen,
    RParen,
    Period,

    EndOfFile,
    Invalid
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
    std::string error;
};

bool isKeyword(TokenType type);
bool isOperator(TokenType type);
bool isDelimiter(TokenType type);
bool isStatementStart(TokenType type);
std::string tokenCategoryName(const Token& token);
std::string tokenTypeDebugName(TokenType type);
