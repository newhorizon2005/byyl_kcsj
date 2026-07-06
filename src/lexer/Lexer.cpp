#include "Lexer.h"

#include <cctype>

Lexer::Lexer(std::string source)
    : source_(std::move(source)),
      keywords_({
          {"const", TokenType::Const},
          {"var", TokenType::Var},
          {"procedure", TokenType::Procedure},
          {"call", TokenType::Call},
          {"begin", TokenType::Begin},
          {"end", TokenType::End},
          {"if", TokenType::If},
          {"then", TokenType::Then},
          {"while", TokenType::While},
          {"do", TokenType::Do},
          {"odd", TokenType::Odd},
          {"read", TokenType::Read},
          {"write", TokenType::Write},
      }) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (!isAtEnd()) {
        skipWhitespaceAndComments(tokens);
        if (isAtEnd()) {
            break;
        }

        const int startLine = line_;
        const int startColumn = column_;
        const char c = peek();

        if (std::isalpha(static_cast<unsigned char>(c))) {
            tokens.push_back(scanIdentifier());
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(scanNumber());
            continue;
        }

        advance();
        switch (c) {
            case '+': tokens.push_back(makeToken(TokenType::Plus, "+", startLine, startColumn)); break;
            case '-': tokens.push_back(makeToken(TokenType::Minus, "-", startLine, startColumn)); break;
            case '*': tokens.push_back(makeToken(TokenType::Times, "*", startLine, startColumn)); break;
            case '/': tokens.push_back(makeToken(TokenType::Slash, "/", startLine, startColumn)); break;
            case '=': tokens.push_back(makeToken(TokenType::Equal, "=", startLine, startColumn)); break;
            case '#': tokens.push_back(makeToken(TokenType::NotEqual, "#", startLine, startColumn)); break;
            case ',': tokens.push_back(makeToken(TokenType::Comma, ",", startLine, startColumn)); break;
            case ';': tokens.push_back(makeToken(TokenType::Semicolon, ";", startLine, startColumn)); break;
            case '(': tokens.push_back(makeToken(TokenType::LParen, "(", startLine, startColumn)); break;
            case ')': tokens.push_back(makeToken(TokenType::RParen, ")", startLine, startColumn)); break;
            case '.': tokens.push_back(makeToken(TokenType::Period, ".", startLine, startColumn)); break;
            case ':':
                if (match('=')) {
                    tokens.push_back(makeToken(TokenType::Assign, ":=", startLine, startColumn));
                } else {
                    tokens.push_back(makeToken(TokenType::Invalid, ":", startLine, startColumn, "非法字符(串)"));
                }
                break;
            case '<':
                if (match('=')) {
                    tokens.push_back(makeToken(TokenType::LessEqual, "<=", startLine, startColumn));
                } else {
                    tokens.push_back(makeToken(TokenType::Less, "<", startLine, startColumn));
                }
                break;
            case '>':
                if (match('=')) {
                    tokens.push_back(makeToken(TokenType::GreaterEqual, ">=", startLine, startColumn));
                } else {
                    tokens.push_back(makeToken(TokenType::Greater, ">", startLine, startColumn));
                }
                break;
            default:
                tokens.push_back(makeToken(TokenType::Invalid, std::string(1, c), startLine, startColumn, "非法字符(串)"));
                break;
        }
    }

    tokens.push_back(makeToken(TokenType::EndOfFile, "", line_, column_));
    return tokens;
}

char Lexer::peek(std::size_t offset) const {
    const std::size_t index = pos_ + offset;
    if (index >= source_.size()) {
        return '\0';
    }
    return source_[index];
}

char Lexer::advance() {
    const char c = source_[pos_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

bool Lexer::match(char expected) {
    if (isAtEnd() || peek() != expected) {
        return false;
    }
    advance();
    return true;
}

void Lexer::skipWhitespaceAndComments(std::vector<Token>& tokens) {
    bool consumed = true;
    while (consumed && !isAtEnd()) {
        consumed = false;
        while (!isAtEnd() && std::isspace(static_cast<unsigned char>(peek()))) {
            advance();
            consumed = true;
        }

        if (peek() == '/' && peek(1) == '/') {
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
            consumed = true;
            continue;
        }

        if (peek() == '/' && peek(1) == '*') {
            const int startLine = line_;
            const int startColumn = column_;
            advance();
            advance();
            bool closed = false;
            while (!isAtEnd()) {
                if (peek() == '*' && peek(1) == '/') {
                    advance();
                    advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                tokens.push_back(makeToken(TokenType::Invalid, "/*", startLine, startColumn, "注释未闭合"));
            }
            consumed = true;
        }
    }
}

Token Lexer::scanIdentifier() {
    const int startLine = line_;
    const int startColumn = column_;
    std::string lexeme;
    while (!isAtEnd() && std::isalnum(static_cast<unsigned char>(peek()))) {
        lexeme.push_back(advance());
    }

    const auto keyword = keywords_.find(lexeme);
    if (keyword != keywords_.end()) {
        return makeToken(keyword->second, lexeme, startLine, startColumn);
    }
    if (lexeme.size() > 8) {
        return makeToken(TokenType::Identifier, lexeme, startLine, startColumn, "标识符长度超长");
    }
    return makeToken(TokenType::Identifier, lexeme, startLine, startColumn);
}

Token Lexer::scanNumber() {
    const int startLine = line_;
    const int startColumn = column_;
    std::string lexeme;
    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        lexeme.push_back(advance());
    }

    if (!isAtEnd() && std::isalpha(static_cast<unsigned char>(peek()))) {
        while (!isAtEnd() && std::isalnum(static_cast<unsigned char>(peek()))) {
            lexeme.push_back(advance());
        }
        return makeToken(TokenType::Invalid, lexeme, startLine, startColumn, "非法字符(串)");
    }

    if (lexeme.size() > 8) {
        return makeToken(TokenType::Number, lexeme, startLine, startColumn, "无符号整数越界");
    }
    return makeToken(TokenType::Number, lexeme, startLine, startColumn);
}

Token Lexer::makeToken(TokenType type, const std::string& lexeme, int line, int column, const std::string& error) const {
    return Token{type, lexeme, line, column, error};
}
