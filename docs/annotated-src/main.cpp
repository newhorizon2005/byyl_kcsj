#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "common/Token.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "parser/SLRParser.h"
#include "semantic/Semantic.h"
#include "target/TargetCode.h"

namespace {

/*
 * main.cpp 是命令行入口和模块调度中心。
 *
 * 公共前置流程：
 *   读取源文件 -> Lexer::tokenize() -> 得到 Token 序列
 *
 * 然后根据 mode 分发：
 *   lex       -> 打印 Token
 *   parse-ll  -> 递归下降语法分析
 *   parse-lr  -> SLR 语法分析并打印分析过程
 *   sem-ll    -> 递归下降 + 语义检查 + 四元式
 *   sem-lr    -> SLR 先验语法检查，通过后再走 sem-ll
 *   target    -> 四元式翻译成目标代码并打印
 *   run       -> 生成目标代码并解释执行
 *   lr-dot    -> 输出 SLR 项目集自动机 DOT
 *   ir-dot    -> 输出四元式控制流图 DOT
 */

// 读取整个源文件内容，返回一个字符串。
std::string readFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("无法打开文件: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

// 打印命令行用法。
void printUsage(const char* program) {
    std::cerr << "用法: " << program << " <lex|parse-ll|parse-lr|sem-ll|sem-lr|target|run|lr-dot|ir-dot> [--stats] <input.pl0>\n";
}

/*
 * 打印词法分析结果。
 *
 * tokens 是 Lexer 输出的完整 Token 序列，包括最后的 EndOfFile。
 * 这里跳过 EOF，只打印源程序中真实出现的单词。
 *
 * stats 为 true 时，额外按中文类别统计数量。
 */
void printLexTokens(const std::vector<Token>& tokens, bool stats) {
    std::map<std::string, int> counts;
    for (const Token& token : tokens) {
        if (token.type == TokenType::EndOfFile) {
            continue;
        }
        const std::string category = tokenCategoryName(token);
        ++counts[category];
        // 错误 Token 需要带行号，便于定位。
        if (!token.error.empty() || token.type == TokenType::Invalid) {
            std::cout << "(" << category << "," << token.lexeme << ",行号:" << token.line << ")\n";
        } else {
            std::cout << "(" << category << "," << token.lexeme << ")\n";
        }
    }

    if (stats) {
        std::cout << "统计:\n";
        for (const auto& item : counts) {
            std::cout << item.first << ": " << item.second << "\n";
        }
    }
}

// 判断 Token 序列中是否存在词法错误。
bool hasLexErrors(const std::vector<Token>& tokens) {
    for (const Token& token : tokens) {
        if (!token.error.empty() || token.type == TokenType::Invalid) {
            return true;
        }
    }
    return false;
}

// 把词法错误打印到标准输出，供 lex/parse/sem 命令使用。
void printLexErrors(const std::vector<Token>& tokens) {
    for (const Token& token : tokens) {
        if (!token.error.empty() || token.type == TokenType::Invalid) {
            std::cout << "(" << tokenCategoryName(token) << "," << token.lexeme << ",行号:" << token.line << ")\n";
        }
    }
}

// 把词法错误打印到指定输出流，target/run/ir-dot 出错时一般写到 std::cerr。
void printLexErrorsTo(std::ostream& output, const std::vector<Token>& tokens) {
    for (const Token& token : tokens) {
        if (!token.error.empty() || token.type == TokenType::Invalid) {
            output << "(" << tokenCategoryName(token) << "," << token.lexeme << ",行号:" << token.line << ")\n";
        }
    }
}

/*
 * 打印语法错误。
 *
 * 当前课程输出格式只要求：
 *   (语法错误,行号:n)
 *
 * 如果同一行有多个语法错误，只打印一次，避免输出太乱。
 */
void printSyntaxErrors(const std::vector<SyntaxError>& errors) {
    if (errors.empty()) {
        std::cout << "语法正确\n";
        return;
    }

    std::set<int> printed;
    for (const SyntaxError& error : errors) {
        if (printed.insert(error.line).second) {
            std::cout << "(语法错误,行号:" << error.line << ")\n";
        }
    }
}

// printSyntaxErrors 的输出流版本。
void printSyntaxErrorsTo(std::ostream& output, const std::vector<SyntaxError>& errors) {
    if (errors.empty()) {
        output << "语法正确\n";
        return;
    }

    std::set<int> printed;
    for (const SyntaxError& error : errors) {
        if (printed.insert(error.line).second) {
            output << "(语法错误,行号:" << error.line << ")\n";
        }
    }
}

// 打印递归下降 Parser 的语法结果。
void printSyntaxResult(const Parser& parser) {
    printSyntaxErrors(parser.syntaxErrors());
}

/*
 * 打印语义分析结果。
 *
 * 如果有语义错误：
 *   只打印语义错误行号。
 *
 * 如果没有语义错误：
 *   1. 打印“语义正确”。
 *   2. 打印中间代码四元式。
 *   3. 打印符号表。
 */
void printSemanticResult(const SemanticAnalyzer& semantic) {
    if (!semantic.errors().empty()) {
        std::set<int> printed;
        for (const SemanticError& error : semantic.errors()) {
            if (printed.insert(error.line).second) {
                std::cout << "(语义错误,行号:" << error.line << ")\n";
            }
        }
        return;
    }

    std::cout << "语义正确\n";
    std::cout << "中间代码:\n";
    // 四元式使用 1 基编号打印，和跳转目标 "$n" 对应。
    int index = 1;
    for (const Quad& quad : semantic.quads()) {
        std::cout << "(" << index << ")(" << quad.op << "," << quad.arg1 << "," << quad.arg2 << "," << quad.result << ")\n";
        ++index;
    }

    std::cout << "符号表:\n";
    for (const Symbol& symbol : semantic.symbolsInOrder()) {
        std::cout << symbolKindName(symbol.kind) << " " << symbol.name;
        if (symbol.kind == SymbolKind::Const) {
            std::cout << " " << symbol.value;
        } else if (symbol.kind == SymbolKind::Var) {
            std::cout << " 0";
        }
        std::cout << "\n";
    }
}

// 把语义错误输出到指定流。
void printSemanticErrorsTo(std::ostream& output, const SemanticAnalyzer& semantic) {
    std::set<int> printed;
    for (const SemanticError& error : semantic.errors()) {
        if (printed.insert(error.line).second) {
            output << "(语义错误,行号:" << error.line << ")\n";
        }
    }
}

// Graphviz DOT 标签转义，避免引号、反斜杠、换行破坏 DOT 格式。
std::string dotEscape(const std::string& text) {
    std::string escaped;
    for (char c : text) {
        switch (c) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\t':
                escaped += "    ";
                break;
            default:
                escaped.push_back(c);
                break;
        }
    }
    return escaped;
}

// 把四元式转成 DOT 图节点标签。
std::string quadToLabel(int index, const Quad& quad) {
    std::ostringstream label;
    label << "(" << index << ")("
          << quad.op << "," << quad.arg1 << "," << quad.arg2 << "," << quad.result << ")";
    return label.str();
}

/*
 * 解析四元式跳转目标。
 *
 * Parser 生成的跳转目标格式是 "$编号"，例如 "$12"。
 * 这个函数把 "$12" 转成整数 12。
 * 如果格式不对，返回 0 表示无效。
 */
int parseQuadTarget(const std::string& target) {
    if (target.size() < 2 || target[0] != '$') {
        return 0;
    }
    try {
        std::size_t consumed = 0;
        int index = std::stoi(target.substr(1), &consumed);
        if (consumed != target.size() - 1) {
            return 0;
        }
        return index;
    } catch (const std::exception&) {
        return 0;
    }
}

// 判断四元式是否是跳转类四元式。约定所有跳转 op 都以 'j' 开头。
bool isJumpQuad(const Quad& quad) {
    return !quad.op.empty() && quad.op[0] == 'j';
}

/*
 * 输出四元式控制流图 DOT。
 *
 * 节点：每条四元式一个节点 Qn。
 * 边：
 * - 普通四元式：连到下一条。
 * - 无条件跳转 j：只连到跳转目标。
 * - 条件跳转：连到跳转目标，同时也连到下一条，表示条件不满足时顺序执行。
 */
void writeIrDot(const std::vector<Quad>& quads, std::ostream& output) {
    output << "digraph IRControlFlow {\n";
    output << "  rankdir=TB;\n";
    output << "  node [shape=box, fontname=\"Menlo\", fontsize=10];\n";
    output << "  edge [fontname=\"Menlo\", fontsize=10];\n\n";

    for (std::size_t i = 0; i < quads.size(); ++i) {
        int index = static_cast<int>(i) + 1;
        output << "  Q" << index << " [label=\"" << dotEscape(quadToLabel(index, quads[i])) << "\"];\n";
    }

    output << "\n";
    for (std::size_t i = 0; i < quads.size(); ++i) {
        int index = static_cast<int>(i) + 1;
        int next = index + 1;
        const Quad& quad = quads[i];

        if (quad.op == "j") {
            int target = parseQuadTarget(quad.result);
            if (target >= 1 && target <= static_cast<int>(quads.size())) {
                output << "  Q" << index << " -> Q" << target << " [label=\"jump\"];\n";
            }
            continue;
        }

        if (isJumpQuad(quad)) {
            int target = parseQuadTarget(quad.result);
            if (target >= 1 && target <= static_cast<int>(quads.size())) {
                output << "  Q" << index << " -> Q" << target
                       << " [label=\"" << dotEscape(quad.op) << "\"];\n";
            }
            if (next <= static_cast<int>(quads.size())) {
                output << "  Q" << index << " -> Q" << next << " [label=\"next\"];\n";
            }
            continue;
        }

        if (next <= static_cast<int>(quads.size())) {
            output << "  Q" << index << " -> Q" << next << ";\n";
        }
    }

    output << "}\n";
}

/*
 * parse-ll 模式：
 *
 * 调用链：
 *   Token 序列 -> Parser(tokens) -> parseProgram() -> 打印语法结果
 *
 * 如果词法阶段已有错误，不进入语法分析。
 */
int runParseLl(const std::vector<Token>& tokens) {
    if (hasLexErrors(tokens)) {
        printLexErrors(tokens);
        return 1;
    }
    Parser parser(tokens);
    parser.parseProgram();
    printSyntaxResult(parser);
    return parser.syntaxErrors().empty() ? 0 : 1;
}

/*
 * sem-ll 模式：
 *
 * 调用链：
 *   Token 序列
 *   -> SemanticAnalyzer semantic
 *   -> Parser(tokens, &semantic)
 *   -> parseProgram()
 *   -> 打印语义结果、四元式、符号表
 *
 * Parser 在识别语法结构时会调用 semantic 进行声明检查和 emit 四元式。
 */
int runSemanticLl(const std::vector<Token>& tokens) {
    if (hasLexErrors(tokens)) {
        printLexErrors(tokens);
        return 1;
    }
    SemanticAnalyzer semantic;
    Parser parser(tokens, &semantic);
    parser.parseProgram();
    if (!parser.syntaxErrors().empty()) {
        printSyntaxResult(parser);
        return 1;
    }
    printSemanticResult(semantic);
    return semantic.errors().empty() ? 0 : 1;
}

/*
 * buildSemantic 是 target/run/ir-dot 共用的“前端构建”函数。
 *
 * 它完成：
 * 1. 词法错误检查。
 * 2. 递归下降语法分析。
 * 3. 语义检查。
 * 4. 四元式生成。
 *
 * 成功返回 true，此时 semantic.quads() 可用。
 */
bool buildSemantic(const std::vector<Token>& tokens, SemanticAnalyzer& semantic, std::ostream& error) {
    if (hasLexErrors(tokens)) {
        printLexErrorsTo(error, tokens);
        return false;
    }

    Parser parser(tokens, &semantic);
    parser.parseProgram();
    if (!parser.syntaxErrors().empty()) {
        printSyntaxErrorsTo(error, parser.syntaxErrors());
        return false;
    }
    if (!semantic.errors().empty()) {
        printSemanticErrorsTo(error, semantic);
        return false;
    }
    return true;
}

/*
 * parse-lr 模式：
 *
 * 调用链：
 *   Token 序列 -> SLRParser -> parse() -> printTrace()
 *
 * 这个模式会打印 SLR 状态栈、符号栈、输入和动作。
 */
int runParseLr(const std::vector<Token>& tokens) {
    if (hasLexErrors(tokens)) {
        printLexErrors(tokens);
        return 1;
    }
    SLRParser parser(tokens);
    parser.parse();
    parser.printTrace(std::cout);
    printSyntaxErrors(parser.syntaxErrors());
    return parser.syntaxErrors().empty() ? 0 : 1;
}

/*
 * sem-lr 模式：
 *
 * 设计策略：
 * 1. 先用 SLRParser 验证语法。
 * 2. 如果 LR 语法通过，再调用 runSemanticLl(tokens) 生成语义结果。
 *
 * 原因：
 * 当前 SLRParser 只做语法分析和追踪，不承担四元式生成。
 * 四元式生成逻辑集中在递归下降 Parser 中，避免两套语义动作重复维护。
 */
int runSemanticLr(const std::vector<Token>& tokens) {
    if (hasLexErrors(tokens)) {
        printLexErrors(tokens);
        return 1;
    }
    SLRParser parser(tokens);
    parser.parse();
    if (!parser.syntaxErrors().empty()) {
        printSyntaxErrors(parser.syntaxErrors());
        return 1;
    }
    return runSemanticLl(tokens);
}

// lr-dot 模式：输出 SLR 项目集自动机 DOT。
int runLrDot() {
    SLRParser::writeDot(std::cout);
    return 0;
}

// ir-dot 模式：先生成四元式，再输出四元式控制流图 DOT。
int runIrDot(const std::vector<Token>& tokens) {
    SemanticAnalyzer semantic;
    if (!buildSemantic(tokens, semantic, std::cerr)) {
        return 1;
    }

    writeIrDot(semantic.quads(), std::cout);
    return 0;
}

// target 模式：先生成四元式，再翻译并打印目标代码。
int runTargetCode(const std::vector<Token>& tokens) {
    SemanticAnalyzer semantic;
    if (!buildSemantic(tokens, semantic, std::cerr)) {
        return 1;
    }

    TargetProgram program = generateTargetProgram(semantic.quads());
    printTargetProgram(program, std::cout);
    return 0;
}

// run 模式：生成目标代码后，用 TargetMachine 解释执行。
int runProgram(const std::vector<Token>& tokens) {
    SemanticAnalyzer semantic;
    if (!buildSemantic(tokens, semantic, std::cerr)) {
        return 1;
    }

    TargetProgram program = generateTargetProgram(semantic.quads());
    return runTargetProgram(program, std::cin, std::cout, std::cerr) ? 0 : 1;
}

}  // namespace

/*
 * 程序入口。
 *
 * 命令格式：
 *   ./pl0c <mode> [--stats] <input.pl0>
 *
 * main 的职责：
 * 1. 解析命令行参数。
 * 2. 读取源文件。
 * 3. 统一执行词法分析。
 * 4. 根据 mode 调用对应 runXXX 函数。
 */
int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    bool stats = false;
    std::string path;

    // 从第 2 个参数开始解析。--stats 是可选开关，其余参数当作输入文件路径。
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--stats") {
            stats = true;
        } else {
            path = arg;
        }
    }
    if (path.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    try {
        // 所有模式都先进行词法分析。
        Lexer lexer(readFile(path));
        std::vector<Token> tokens = lexer.tokenize();

        // 根据 mode 分发到不同功能。
        if (mode == "lex") {
            printLexTokens(tokens, stats);
            return hasLexErrors(tokens) ? 1 : 0;
        }
        if (mode == "parse-ll") {
            return runParseLl(tokens);
        }
        if (mode == "sem-ll") {
            return runSemanticLl(tokens);
        }
        if (mode == "parse-lr") {
            return runParseLr(tokens);
        }
        if (mode == "sem-lr") {
            return runSemanticLr(tokens);
        }
        if (mode == "target") {
            return runTargetCode(tokens);
        }
        if (mode == "run") {
            return runProgram(tokens);
        }
        if (mode == "lr-dot") {
            return runLrDot();
        }
        if (mode == "ir-dot") {
            return runIrDot(tokens);
        }

        printUsage(argv[0]);
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
}
