#!/usr/bin/env bash
# dev.sh — one-command build+run for this repo.
#
#   dev all                     build+run every tests/*.cpp with warnings + ASan/UBSan
#   dev tests/order_book.cpp    build+run just that file
#   dev order_book              same, bare name resolves to tests/order_book.cpp
#   dev all -r                  release build (-O2 -DNDEBUG), no sanitizers
#   dev all -n                  build only, don't run
#   dev tidy [path]             clang-tidy (default: include/)
#
#   CXX=clang++ dev all         switch compiler
#   INC_DIR=src TEST_DIR=test   reuse in another project
set -euo pipefail

ROOT="${KCPP_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"

CXX="${CXX:-g++}"
STD="-std=c++20"
INC="$ROOT/${INC_DIR:-include}"
TESTS="$ROOT/${TEST_DIR:-tests}"
OUT="$ROOT/${OUT_DIR:-build/dev}"

WARN=(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
      -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wnon-virtual-dtor)
SAN=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
DBG=(-g -O1 -D_GLIBCXX_ASSERTIONS)
REL=(-O2 -DNDEBUG)

if [[ -t 2 ]]; then
  c_info=$'\033[36m'; c_err=$'\033[31m'; c_off=$'\033[0m'
else
  c_info=""; c_err=""; c_off=""
fi

usage() {
  sed -n '2,12p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

die() { printf '%serror:%s %s\n' "$c_err" "$c_off" "$*" >&2; exit 1; }

resolve() { # $1 -> prints zero or more absolute source paths
  local t="$1"
  if [[ "$t" == all ]]; then
    local f
    for f in "$TESTS"/*.cpp; do
      [[ -e "$f" ]] || break
      printf '%s\n' "$f"
    done
    return
  fi
  local cand
  for cand in "$t" "$ROOT/$t" "$TESTS/${t%.cpp}.cpp" "$ROOT/${t%.cpp}.cpp"; do
    if [[ -f "$cand" ]]; then
      printf '%s\n' "$(cd "$(dirname "$cand")" && pwd)/$(basename "$cand")"
      return
    fi
  done
}

build_one() { # $1 = source path -> prints binary path
  local src="$1" name bin
  name="$(basename "$src" .cpp)"
  bin="$OUT/$name"

  local flags=("$STD" -I"$INC" "${WARN[@]}")
  if [[ "$MODE" == rel ]]; then
    flags+=("${REL[@]}")
  else
    flags+=("${DBG[@]}" "${SAN[@]}")
  fi

  printf '%s▶ build %s%s (%s)\n' "$c_info" "$name" "$c_off" "$MODE" >&2
  if ! "$CXX" "${flags[@]}" "$src" -o "$bin"; then
    die "compile failed: $name"
  fi
  printf '%s\n' "$bin"
}

tidy() { # $1 = optional path
  command -v clang-tidy >/dev/null || die "clang-tidy not installed"
  local target="${1:-$INC}"
  [[ -d "$target" ]] || target="$ROOT/$target"
  [[ -d "$target" ]] || die "no such directory: ${1:-}"
  find "$target" -maxdepth 2 \( -name '*.hpp' -o -name '*.cpp' \) | while read -r f; do
    printf '%s▶ tidy %s%s\n' "$c_info" "$(basename "$f")" "$c_off" >&2
    clang-tidy "$f" \
      -checks='bugprone-*,clang-analyzer-*,performance-*,readability-redundant-*' \
      -- "$STD" -I"$INC" 2>&1 | sed 's/^/  /'
  done
}

MODE=san
RUN=1
ARGS=()

for a in "$@"; do
  case "$a" in
    -r|--release) MODE=rel ;;
    -n|--no-run)  RUN=0 ;;
    -h|--help)    usage; exit 0 ;;
    -*)           die "unknown flag: $a" ;;
    *)            ARGS+=("$a") ;;
  esac
done

[[ ${#ARGS[@]} -gt 0 ]] || { usage >&2; exit 1; }

TARGET="${ARGS[0]}"

if [[ "$TARGET" == tidy ]]; then
  tidy "${ARGS[1]:-}"
  exit 0
fi

mkdir -p "$OUT"

srcs=()
while read -r s; do srcs+=("$s"); done < <(resolve "$TARGET")
[[ ${#srcs[@]} -gt 0 ]] || die "cannot resolve source: $TARGET"

bins=()
for s in "${srcs[@]}"; do
  bins+=("$(build_one "$s")")
done

[[ "$RUN" == 1 ]] || exit 0

fail=0
for bin in "${bins[@]}"; do
  name="$(basename "$bin")"
  log="$OUT/$name.log"
  printf '%s▶ run %s%s\n' "$c_info" "$name" "$c_off" >&2
  if "$bin" >"$log" 2>&1; then
    grep -E 'test cases:|Status:' "$log" | tail -2 | sed 's/^/    /'
  else
    printf '%sFAIL%s %s  (log: %s)\n' "$c_err" "$c_off" "$name" "$log" >&2
    tail -20 "$log" | sed 's/^/    /'
    fail=1
  fi
done

exit "$fail"
