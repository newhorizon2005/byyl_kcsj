#include "Lexer.h"

#include <cctype>

/*
 * 构造 Lexer。
 *
 * : source_(std::move(source))
 *   把传入的源代码字符串移动进成员变量，避免不必要拷贝。
 *
 * keywords_(...)
 *   建立“保留字字符串 -> TokenType”的表。
 *   scanIdentifier() 先读出一个字母数字串，再查这张表：
 *   - 查到：说明它是 const/var/if/while 等保留字。
 *   - 查不到：说明它是普通标识符。
 */
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

/*
 * tokenize 是词法分析主循环。
 *
 * 总体算法：
 * 1. 创建空 Token 数组 tokens。
 * 2. 只要没到源代码末尾，就不断扫描。
 * 3. 每轮先跳过空白和注释。
 * 4. 看当前位置第一个字符，决定进入哪种识别逻辑：
 *    - 字母开头：标识符或保留字。
 *    - 数字开头：无符号整数。
 *    - 其他字符：运算符、界符或非法字符。
 * 5. 最后追加 EndOfFile，方便语法分析器判断结束。
 */
std::vector<Token> Lexer::tokenize() {
    // 保存扫描结果。后续 Parser 不再读取原始字符，而是读取这个数组。
    std::vector<Token> tokens;

    // 主循环：每次至少消费一个字符，直到 pos_ 到达 source_.size()。
    while (!isAtEnd()) {
        // 先处理空格、换行、单行注释、多行注释。
        // 多行注释如果未闭合，会在 tokens 里产生错误 Token。
        skipWhitespaceAndComments(tokens);

        // 跳过空白/注释后可能刚好到达文件末尾，此时退出。
        if (isAtEnd()) {
            break;
        }

        // 记录当前 Token 的起始位置。后面 advance() 会改变 line_/column_，
        // 所以必须在消费字符前保存。
        const int startLine = line_;
        const int startColumn = column_;

        // 查看当前字符但不移动指针。
        const char c = peek();

        // 标识符和保留字都以字母开头，例如 x、count1、while。
        if (std::isalpha(static_cast<unsigned char>(c))) {
            tokens.push_back(scanIdentifier());
            continue;
        }

        // 数字字面量以数字开头，例如 0、123。
        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(scanNumber());
            continue;
        }

        // 到这里说明当前字符不是字母也不是数字。
        // 先消费当前字符，再根据字符值决定生成哪类 Token。
        advance();
        switch (c) {
            // 单字符运算符或界符可以直接生成 Token。
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
                // PL/0 的赋值符号是两个字符 ":="。
                // advance() 已经消费了 ':'，这里用 match('=') 尝试消费后面的 '='。
                if (match('=')) {
                    tokens.push_back(makeToken(TokenType::Assign, ":=", startLine, startColumn));
                } else {
                    // 单独一个 ':' 在本语言中没有意义，所以记为非法字符。
                    tokens.push_back(makeToken(TokenType::Invalid, ":", startLine, startColumn, "非法字符(串)"));
                }
                break;
            case '<':
                // '<' 和 '<=' 都合法：如果后面跟 '='，就是 LessEqual，否则是 Less。
                if (match('=')) {
                    tokens.push_back(makeToken(TokenType::LessEqual, "<=", startLine, startColumn));
                } else {
                    tokens.push_back(makeToken(TokenType::Less, "<", startLine, startColumn));
                }
                break;
            case '>':
                // '>' 和 '>=' 同理。
                if (match('=')) {
                    tokens.push_back(makeToken(TokenType::GreaterEqual, ">=", startLine, startColumn));
                } else {
                    tokens.push_back(makeToken(TokenType::Greater, ">", startLine, startColumn));
                }
                break;
            default:
                // 所有未被语言定义的字符都走到这里，例如 @、$。
                tokens.push_back(makeToken(TokenType::Invalid, std::string(1, c), startLine, startColumn, "非法字符(串)"));
                break;
        }
    }

    // EOF 不是源代码真实字符，而是给 Parser 使用的结束标记。
    tokens.push_back(makeToken(TokenType::EndOfFile, "", line_, column_));
    return tokens;
}

/*
 * peek 只查看字符，不移动扫描位置。
 *
 * offset = 0 表示当前字符；
 * offset = 1 表示下一个字符。
 *
 * 如果越界，返回 '\0' 作为哨兵字符，避免访问 source_ 越界。
 */
char Lexer::peek(std::size_t offset) const {
    const std::size_t index = pos_ + offset;
    if (index >= source_.size()) {
        return '\0';
    }
    return source_[index];
}

/*
 * advance 消费一个字符。
 *
 * 执行顺序：
 * 1. 取出 source_[pos_]。
 * 2. pos_ 自增，表示扫描指针向后走了一格。
 * 3. 如果消费的是换行符，行号加一，列号回到 1。
 * 4. 否则列号加一。
 * 5. 返回被消费的字符，供调用方加入 lexeme。
 */
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

// 判断是否已经扫描完整个源代码字符串。
bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

/*
 * match 是处理双字符符号的工具函数。
 *
 * 例如 tokenize() 已经读到了 '<'，接下来想判断是不是 '<='：
 * - 如果当前位置是 '='，match 会消费 '=' 并返回 true。
 * - 否则保持当前位置不变，返回 false。
 */
bool Lexer::match(char expected) {
    if (isAtEnd() || peek() != expected) {
        return false;
    }
    advance();
    return true;
}

/*
 * 跳过空白和注释。
 *
 * 这个函数稍微绕一点，因为“跳过空白后可能遇到注释，跳过注释后又可能遇到空白”，
 * 所以用 consumed 控制外层循环：只要本轮确实消耗过东西，就再检查一遍。
 *
 * tokens 参数的作用：
 * - 正常空白/注释不会生成 Token。
 * - 如果多行注释没有闭合，需要生成一个错误 Token，因此需要能写入 tokens。
 */
void Lexer::skipWhitespaceAndComments(std::vector<Token>& tokens) {
    bool consumed = true;
    while (consumed && !isAtEnd()) {
        // 每轮先假设没有消耗字符，如果下面任何分支消耗了字符，再把它改成 true。
        consumed = false;

        // 跳过空格、制表符、换行等所有 C 标准空白字符。
        while (!isAtEnd() && std::isspace(static_cast<unsigned char>(peek()))) {
            advance();
            consumed = true;
        }

        // 单行注释：从 // 开始，到换行前结束。
        if (peek() == '/' && peek(1) == '/') {
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
            consumed = true;
            continue;
        }

        // 多行注释：从 /* 开始，到 */ 结束。
        if (peek() == '/' && peek(1) == '*') {
            const int startLine = line_;
            const int startColumn = column_;

            // 消费 '/' 和 '*'。
            advance();
            advance();
            bool closed = false;

            // 不断向后找成对的 */。
            while (!isAtEnd()) {
                if (peek() == '*' && peek(1) == '/') {
                    // 找到注释结束符，消费 '*' 和 '/'。
                    advance();
                    advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                // 文件结束都没找到 */，生成词法错误。
                tokens.push_back(makeToken(TokenType::Invalid, "/*", startLine, startColumn, "注释未闭合"));
            }
            consumed = true;
        }
    }
}

/*
 * 扫描标识符或保留字。
 *
 * 对应的词法规则可以理解为：
 *
 *     identifier_or_keyword -> letter (letter | digit)*
 *
 * 执行流程：
 * 1. 保存起始行列。
 * 2. 只要当前位置还是字母或数字，就消费并追加到 lexeme。
 * 3. 扫描结束后，用 lexeme 查保留字表。
 * 4. 如果是保留字，返回对应保留字 Token。
 * 5. 否则检查普通标识符长度，超过 8 位则保留 Identifier 类型但附带错误信息。
 */
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

/*
 * 扫描无符号整数。
 *
 * 对应的正常词法规则：
 *
 *     number -> digit+
 *
 * 本项目额外处理两个错误：
 * 1. 数字后直接接字母，例如 123abc。这个整体被认为是非法字符(串)。
 * 2. 数字长度超过 8 位，标记为“无符号整数越界”。
 */
Token Lexer::scanNumber() {
    const int startLine = line_;
    const int startColumn = column_;
    std::string lexeme;

    // 先把连续数字全部读进 lexeme。
    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        lexeme.push_back(advance());
    }

    // 如果数字后面紧跟字母，继续把后续字母数字也吞掉，
    // 这样 123abc 会作为一个整体错误输出，而不是拆成 123 和 abc。
    if (!isAtEnd() && std::isalpha(static_cast<unsigned char>(peek()))) {
        while (!isAtEnd() && std::isalnum(static_cast<unsigned char>(peek()))) {
            lexeme.push_back(advance());
        }
        return makeToken(TokenType::Invalid, lexeme, startLine, startColumn, "非法字符(串)");
    }

    // 课程要求中通常会限制整数长度，这里超过 8 位视为越界。
    if (lexeme.size() > 8) {
        return makeToken(TokenType::Number, lexeme, startLine, startColumn, "无符号整数越界");
    }
    return makeToken(TokenType::Number, lexeme, startLine, startColumn);
}

/*
 * makeToken 是一个很小的工厂函数。
 *
 * 好处：
 * - 所有 Token 创建格式统一。
 * - 如果以后 Token 结构增加字段，只需要集中调整这里和少量调用处。
 */
Token Lexer::makeToken(TokenType type, const std::string& lexeme, int line, int column, const std::string& error) const {
    return Token{type, lexeme, line, column, error};
}
