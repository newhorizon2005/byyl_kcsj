# PL/0 编译原理课程设计使用说明

本项目是一个 C++17 实现的 PL/0 编译器课程设计，主要用于完成课程要求中的词法分析、语法分析、语义分析、四元式生成、伪目标代码生成和解释执行。

当前已经实现：

- 手写 PL/0 词法分析器
- 递归下降语法分析器
- SLR 自底向上语法分析器
- 符号表和基础语义检查
- 四元式中间代码生成
- 四元式到伪目标代码生成
- 伪目标代码解释执行
- SLR 项目集状态图导出
- 四元式控制流图导出
- Lex/Yacc 三个入门实验

当前收尾验收状态：

- `tests/run_all.sh`：12/12 passed
- 主构建产物：`cmake-build/pl0c`

学习代码时建议配合阅读：[docs/STUDY_NOTES.html](docs/STUDY_NOTES.html)。

任务进度表见：[docs/TASKS.md](docs/TASKS.md)。

## 1. 环境要求

本项目统一使用 CMake 构建。

主编译器构建需要：

- `cmake`
- C++17 编译器，例如 macOS 上的 `clang++` / `g++`

Lex/Yacc 入门实验另外需要：

- `flex`，用于 Lex 实验
- `bison`，用于 Yacc/Bison 实验

检查命令：

```bash
cmake --version
g++ --version
flex --version
bison --version
```

## 2. 构建项目

在项目根目录执行：

```bash
cmake -S . -B cmake-build
cmake --build cmake-build
```

构建成功后会生成：

```text
cmake-build/pl0c
```

如果要重新构建，可以先删除构建目录：

```bash
rm -rf cmake-build
cmake -S . -B cmake-build
cmake --build cmake-build
```

## 3. 运行主编译器

命令格式：

```bash
./cmake-build/pl0c <模式> [--stats] <PL/0源文件>
```

支持的模式如下：

| 模式 | 作用 |
| --- | --- |
| `lex` | 只执行词法分析，输出 Token 序列。 |
| `parse-ll` | 使用递归下降语法分析器检查语法。 |
| `parse-lr` | 使用 SLR 语法分析器检查语法，并输出移进/归约过程。 |
| `sem-ll` | 使用递归下降分析并执行语义检查、生成四元式。 |
| `sem-lr` | 先使用 SLR 检查语法，再执行语义检查、生成四元式。 |
| `target` | 把四元式翻译成伪目标代码。 |
| `run` | 解释执行伪目标代码，能看到程序运行输出。 |
| `lr-dot` | 导出 SLR 项目集状态转换图 DOT 文件。 |
| `ir-dot` | 导出四元式控制流图 DOT 文件。 |

## 4. 常用命令

词法分析：

```bash
./cmake-build/pl0c lex tests/samples/valid_sample.pl0
```

词法分析并输出分类统计：

```bash
./cmake-build/pl0c lex --stats tests/samples/valid_sample.pl0
```

递归下降语法分析：

```bash
./cmake-build/pl0c parse-ll tests/samples/valid_sample.pl0
```

SLR 语法分析：

```bash
./cmake-build/pl0c parse-lr tests/samples/valid_sample.pl0
```

语义分析和四元式生成：

```bash
./cmake-build/pl0c sem-ll tests/samples/valid_sample.pl0
./cmake-build/pl0c sem-lr tests/samples/valid_sample.pl0
```

四元式到目标代码：

```bash
./cmake-build/pl0c target tests/samples/valid_sample.pl0
```

运行程序。样例中有 `read(x, y)`，所以需要通过标准输入给两个整数：

```bash
printf '1 3\n' | ./cmake-build/pl0c run tests/samples/valid_sample.pl0
```

输出结果：

```text
3
2
1
21
```

导出 SLR 项目集状态图：

```bash
./cmake-build/pl0c lr-dot tests/samples/valid_sample.pl0 > docs/diagrams/slr_states.dot
```

导出四元式控制流图：

```bash
./cmake-build/pl0c ir-dot tests/samples/valid_sample.pl0 > docs/diagrams/ir_cfg.dot
```

如果安装了 Graphviz，可以把 DOT 文件转换成图片：

```bash
dot -Tpng docs/diagrams/slr_states.dot -o docs/diagrams/slr_states.png
dot -Tpng docs/diagrams/ir_cfg.dot -o docs/diagrams/ir_cfg.png
```

## 5. 测试

运行 CMake 测试目标：

```bash
cmake --build cmake-build --target check
```

或者直接运行测试脚本：

```bash
tests/run_all.sh
```

当前测试覆盖：

- 正确 PL/0 程序
- 词法错误
- 语法错误
- 语义错误
- LL 语法分析
- SLR 语法分析
- 四元式生成
- 目标代码生成
- 目标代码解释执行
- SLR 状态图导出
- 四元式控制流图导出

测试样例目录：

```text
tests/samples/
```

主要样例：

| 文件 | 作用 |
| --- | --- |
| `valid_sample.pl0` | 正确程序，用于验证完整流程。 |
| `lex_errors.pl0` | 词法错误样例。 |
| `syntax_errors.pl0` | 语法错误样例。 |
| `semantic_errors.pl0` | 语义错误样例。 |

## 6. Lex/Yacc 实验

课程要求中的 Lex/Yacc 小实验放在：

```text
lex-yacc-labs/
```

### 6.1 字符频率统计

```bash
cd lex-yacc-labs/01-letter-frequency
flex letter_frequency.l
cc lex.yy.c -o letter_frequency
printf 'KHOOR ZRUOG\n' | ./letter_frequency
```

### 6.2 单词、数字和符号识别

```bash
cd lex-yacc-labs/02-token-recognizer
flex token_recognizer.l
cc lex.yy.c -o token_recognizer
printf 'int year = 2023;' | ./token_recognizer
```

### 6.3 加法乘法计算器

```bash
cd lex-yacc-labs/03-calculator
bison -d -o calculator.tab.c calculator.y
flex -o calculator.lex.c calculator.l
cc calculator.tab.c calculator.lex.c -o calculator
printf '5*7+2\n9+3*6\n' | ./calculator
```

## 7. 项目目录

```text
src/
  common/       Token 类型和通用工具
  lexer/        词法分析器
  parser/       递归下降分析器和 SLR 分析器
  semantic/     符号表、语义检查、四元式
  target/       伪目标代码生成和解释执行

tests/
  samples/      测试用例
  run_all.sh    批量测试脚本

docs/
  TASKS.md        任务计划和完成状态
  STUDY_NOTES.html  代码学习笔记
  diagrams/       DOT 可视化文件

lex-yacc-labs/
  01-letter-frequency/
  02-token-recognizer/
  03-calculator/
```

## 8. 建议学习顺序

1. 先运行 `lex`，理解 Token 是怎么输出的。
2. 再看 `parse-ll`，理解递归下降如何按文法调用函数。
3. 再看 `sem-ll`，理解符号表和四元式如何生成。
4. 再看 `target` 和 `run`，理解四元式如何变成可执行的伪目标代码。
5. 再看 `parse-lr`，理解 SLR 的状态栈、符号栈、移进和归约。
6. 最后看 `lr-dot` 和 `ir-dot`，理解可视化图和内部数据结构的对应关系。

详细代码学习路线见：[docs/STUDY_NOTES.html](docs/STUDY_NOTES.html)。
