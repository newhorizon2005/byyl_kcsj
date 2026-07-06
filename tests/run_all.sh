#!/usr/bin/env bash
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

PL0C="${PL0C:-$REPO_ROOT/cmake-build/pl0c}"
EXPECT_PARSE_LR="${EXPECT_PARSE_LR:-implemented}"

SAMPLE_DIR="$SCRIPT_DIR/samples"
VALID_SAMPLE="$SAMPLE_DIR/valid_sample.pl0"
LEX_ERRORS_SAMPLE="$SAMPLE_DIR/lex_errors.pl0"
SYNTAX_ERRORS_SAMPLE="$SAMPLE_DIR/syntax_errors.pl0"
SEMANTIC_ERRORS_SAMPLE="$SAMPLE_DIR/semantic_errors.pl0"

total=0
passed=0
failed=0

last_output=""
last_status=0
missing_needles=""

record_pass() {
  local name="$1"
  total=$((total + 1))
  passed=$((passed + 1))
  printf 'PASS %s\n' "$name"
}

record_fail() {
  local name="$1"
  local reason="$2"
  total=$((total + 1))
  failed=$((failed + 1))
  printf 'FAIL %s\n' "$name"
  printf '     %s\n' "$reason"
}

print_output_excerpt() {
  if [[ -z "$last_output" ]]; then
    printf '     | <no output>\n'
    return
  fi

  printf '%s\n' "$last_output" | sed -n '1,20p' | sed 's/^/     | /'
}

run_pl0c() {
  local mode="$1"
  local sample="$2"

  last_output="$("$PL0C" "$mode" "$sample" 2>&1)"
  last_status=$?
}

run_pl0c_with_input() {
  local mode="$1"
  local sample="$2"
  local input="$3"

  last_output="$(printf '%s' "$input" | "$PL0C" "$mode" "$sample" 2>&1)"
  last_status=$?
}

require_output() {
  local needle

  missing_needles=""
  for needle in "$@"; do
    if [[ "$last_output" != *"$needle"* ]]; then
      if [[ -n "$missing_needles" ]]; then
        missing_needles="$missing_needles, $needle"
      else
        missing_needles="$needle"
      fi
    fi
  done

  [[ -z "$missing_needles" ]]
}

expect_success() {
  local name="$1"
  local mode="$2"
  local sample="$3"
  shift 3

  run_pl0c "$mode" "$sample"
  if [[ "$last_status" -ne 0 ]]; then
    record_fail "$name" "expected exit 0, got $last_status"
    print_output_excerpt
    return
  fi

  if ! require_output "$@"; then
    record_fail "$name" "missing expected output: $missing_needles"
    print_output_excerpt
    return
  fi

  record_pass "$name"
}

expect_failure() {
  local name="$1"
  local mode="$2"
  local sample="$3"
  shift 3

  run_pl0c "$mode" "$sample"
  if [[ "$last_status" -eq 0 ]]; then
    record_fail "$name" "expected non-zero exit, got 0"
    print_output_excerpt
    return
  fi

  if ! require_output "$@"; then
    record_fail "$name" "missing expected output: $missing_needles"
    print_output_excerpt
    return
  fi

  record_pass "$name"
}

expect_success_with_input_exact() {
  local name="$1"
  local mode="$2"
  local sample="$3"
  local input="$4"
  local expected="$5"

  run_pl0c_with_input "$mode" "$sample" "$input"
  if [[ "$last_status" -ne 0 ]]; then
    record_fail "$name" "expected exit 0, got $last_status"
    print_output_excerpt
    return
  fi

  if [[ "$last_output" != "$expected" ]]; then
    record_fail "$name" "unexpected output"
    printf '     expected: %q\n' "$expected"
    printf '     actual:   %q\n' "$last_output"
    return
  fi

  record_pass "$name"
}

check_parse_lr() {
  local name="parse-lr valid_sample"

  run_pl0c "parse-lr" "$VALID_SAMPLE"

  case "$EXPECT_PARSE_LR" in
    implemented)
      if [[ "$last_status" -ne 0 ]]; then
        record_fail "$name" "expected implemented LR parser to accept valid_sample with exit 0, got $last_status"
        print_output_excerpt
        return
      fi

      if ! require_output "语法正确"; then
        record_fail "$name" "missing expected output after LR implementation: $missing_needles"
        print_output_excerpt
        return
      fi

      record_pass "$name"
      ;;
    pending)
      if [[ "$last_status" -eq 0 ]] && require_output "语法正确"; then
        record_pass "$name"
        return
      fi

      if [[ "$last_status" -ne 0 ]] && require_output "暂未实现"; then
        record_pass "$name (pending implementation)"
        return
      fi

      record_fail "$name" "expected either successful LR parse or the current not-implemented message; set EXPECT_PARSE_LR=implemented once LR lands"
      print_output_excerpt
      ;;
    *)
      record_fail "$name" "invalid EXPECT_PARSE_LR=$EXPECT_PARSE_LR; use pending or implemented"
      ;;
  esac
}

finish() {
  printf '\nSummary: %d/%d passed\n' "$passed" "$total"
  if [[ "$failed" -ne 0 ]]; then
    exit 1
  fi
}

if [[ ! -x "$PL0C" ]]; then
  record_fail "preflight pl0c" "expected executable at $PL0C; run: cmake -S . -B cmake-build && cmake --build cmake-build, or set PL0C=/path/to/pl0c"
  finish
fi

for sample in "$VALID_SAMPLE" "$LEX_ERRORS_SAMPLE" "$SYNTAX_ERRORS_SAMPLE" "$SEMANTIC_ERRORS_SAMPLE"; do
  if [[ ! -f "$sample" ]]; then
    record_fail "preflight sample files" "missing sample file: $sample"
  fi
done

if [[ "$failed" -ne 0 ]]; then
  finish
fi

expect_success "lex valid_sample" "lex" "$VALID_SAMPLE" \
  "(保留字,const)" \
  "(标识符,max)" \
  "(界符,.)"

expect_failure "lex lex_errors" "lex" "$LEX_ERRORS_SAMPLE" \
  "标识符长度超长" \
  "无符号整数越界" \
  "非法字符(串),12abc" \
  "非法字符(串),@"

expect_success "parse-ll valid_sample" "parse-ll" "$VALID_SAMPLE" \
  "语法正确"

expect_failure "parse-ll syntax_errors" "parse-ll" "$SYNTAX_ERRORS_SAMPLE" \
  "(语法错误,行号:1)" \
  "(语法错误,行号:4)" \
  "(语法错误,行号:5)"

expect_success "sem-ll valid_sample" "sem-ll" "$VALID_SAMPLE" \
  "语义正确" \
  "中间代码:" \
  "符号表:"

expect_success "sem-lr valid_sample" "sem-lr" "$VALID_SAMPLE" \
  "语义正确" \
  "中间代码:" \
  "符号表:"

expect_success "lr-dot grammar" "lr-dot" "$VALID_SAMPLE" \
  "digraph SLRStates" \
  "I0" \
  "->"

expect_success "ir-dot valid_sample" "ir-dot" "$VALID_SAMPLE" \
  "digraph IRControlFlow" \
  "Q1" \
  "Q16 -> Q21"

expect_success "target valid_sample" "target" "$VALID_SAMPLE" \
  "JMP MAIN" \
  "PROC add" \
  "CALL countdn" \
  "WRITE sum"

expect_success_with_input_exact "run valid_sample input 1 3" "run" "$VALID_SAMPLE" \
  $'1 3\n' \
  $'3\n2\n1\n21'

expect_failure "sem-ll semantic_errors" "sem-ll" "$SEMANTIC_ERRORS_SAMPLE" \
  "(语义错误,行号:1)" \
  "(语义错误,行号:7)" \
  "(语义错误,行号:13)"

check_parse_lr

finish
