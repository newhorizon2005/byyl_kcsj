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

std::string readFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("无法打开文件: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void printUsage(const char* program) {
    std::cerr << "用法: " << program << " <lex|parse-ll|parse-lr|sem-ll|sem-lr|target|run|lr-dot|ir-dot> [--stats] <input.pl0>\n";
}

void printLexTokens(const std::vector<Token>& tokens, bool stats) {
    std::map<std::string, int> counts;
    for (const Token& token : tokens) {
        if (token.type == TokenType::EndOfFile) {
            continue;
        }
        const std::string category = tokenCategoryName(token);
        ++counts[category];
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

bool hasLexErrors(const std::vector<Token>& tokens) {
    for (const Token& token : tokens) {
        if (!token.error.empty() || token.type == TokenType::Invalid) {
            return true;
        }
    }
    return false;
}

void printLexErrors(const std::vector<Token>& tokens) {
    for (const Token& token : tokens) {
        if (!token.error.empty() || token.type == TokenType::Invalid) {
            std::cout << "(" << tokenCategoryName(token) << "," << token.lexeme << ",行号:" << token.line << ")\n";
        }
    }
}

void printLexErrorsTo(std::ostream& output, const std::vector<Token>& tokens) {
    for (const Token& token : tokens) {
        if (!token.error.empty() || token.type == TokenType::Invalid) {
            output << "(" << tokenCategoryName(token) << "," << token.lexeme << ",行号:" << token.line << ")\n";
        }
    }
}

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

void printSyntaxResult(const Parser& parser) {
    printSyntaxErrors(parser.syntaxErrors());
}

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

void printSemanticErrorsTo(std::ostream& output, const SemanticAnalyzer& semantic) {
    std::set<int> printed;
    for (const SemanticError& error : semantic.errors()) {
        if (printed.insert(error.line).second) {
            output << "(语义错误,行号:" << error.line << ")\n";
        }
    }
}

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

std::string quadToLabel(int index, const Quad& quad) {
    std::ostringstream label;
    label << "(" << index << ")("
          << quad.op << "," << quad.arg1 << "," << quad.arg2 << "," << quad.result << ")";
    return label.str();
}

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

bool isJumpQuad(const Quad& quad) {
    return !quad.op.empty() && quad.op[0] == 'j';
}

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

int runLrDot() {
    SLRParser::writeDot(std::cout);
    return 0;
}

int runIrDot(const std::vector<Token>& tokens) {
    SemanticAnalyzer semantic;
    if (!buildSemantic(tokens, semantic, std::cerr)) {
        return 1;
    }

    writeIrDot(semantic.quads(), std::cout);
    return 0;
}

int runTargetCode(const std::vector<Token>& tokens) {
    SemanticAnalyzer semantic;
    if (!buildSemantic(tokens, semantic, std::cerr)) {
        return 1;
    }

    TargetProgram program = generateTargetProgram(semantic.quads());
    printTargetProgram(program, std::cout);
    return 0;
}

int runProgram(const std::vector<Token>& tokens) {
    SemanticAnalyzer semantic;
    if (!buildSemantic(tokens, semantic, std::cerr)) {
        return 1;
    }

    TargetProgram program = generateTargetProgram(semantic.quads());
    return runTargetProgram(program, std::cin, std::cout, std::cerr) ? 0 : 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    bool stats = false;
    std::string path;
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
        Lexer lexer(readFile(path));
        std::vector<Token> tokens = lexer.tokenize();

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
