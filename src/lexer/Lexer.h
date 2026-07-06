#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "../common/Token.h"

class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> tokenize();

private:
    char peek(std::size_t offset = 0) const;
    char advance();
    bool isAtEnd() const;
    bool match(char expected);
    void skipWhitespaceAndComments(std::vector<Token>& tokens);

    Token scanIdentifier();
    Token scanNumber();
    Token makeToken(TokenType type, const std::string& lexeme, int line, int column, const std::string& error = "") const;

    std::string source_;
    std::size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
    std::unordered_map<std::string, TokenType> keywords_;
};
