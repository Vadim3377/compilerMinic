#!/bin/bash
# Extra tests for the MiniC compiler, on top of the supplied tests/ folder.
#
# Usage, from the code directory:   ./mytests/run_mytests.sh
#
# Valid programs: each folder mytests/<name>/ has <name>.c and a driver.cpp,
# the same layout as tests/. The program is compiled in a fresh temporary
# directory (as the marking scripts do), the IR is checked with llvm-as and
# opt, linked with the driver and run. The driver prints PASSED/FAILED for
# each check and exits non-zero if any check failed.
#
# Invalid programs: each file in mytests/errors/ starts with a line
# "// EXPECT: <text>". The compiler must exit with a non-zero status, and its
# error message must contain <text> (for example "arity" or "Syntax error").
#
# The toolchain setup is copied from the supplied tests/tests.sh, so on DCS
# the same LLVM 23.1.0 clang++, llvm-as and opt are used as there. Elsewhere
# (when /modules/cs325 does not exist) the tools on the PATH are used.

LLVM_INSTALL_PATH=/modules/cs325/llvm-23.1.0

if ! command -v module >/dev/null 2>&1 && [ -f /etc/profile.d/modules.sh ]; then
  source /etc/profile.d/modules.sh
fi

if command -v module >/dev/null 2>&1; then
  module load GCC/13.3.0
fi

if [ -d "$LLVM_INSTALL_PATH" ]; then
  export PATH=$LLVM_INSTALL_PATH/bin:$PATH
  export LD_LIBRARY_PATH=$LLVM_INSTALL_PATH/lib:$LD_LIBRARY_PATH
  LINK_CXX=$LLVM_INSTALL_PATH/bin/clang++
  LLVM_AS=$LLVM_INSTALL_PATH/bin/llvm-as
  OPT=$LLVM_INSTALL_PATH/bin/opt
else
  LINK_CXX=${LINK_CXX:-clang++}
  LLVM_AS=${LLVM_AS:-llvm-as}
  OPT=${OPT:-opt}
fi

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1
make -s mccomp || exit 1

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

pass=0
fail=0

echo "***** Valid programs *****"
for dir in mytests/*/; do
  name=$(basename "$dir")
  if [ "$name" = "errors" ]; then
    continue
  fi
  rm -f "$WORK"/*
  cd "$WORK" || exit 1

  if ! "$ROOT/mccomp" "$ROOT/$dir/$name.c" > compile.log 2>&1; then
    echo "FAIL $name: compiler exited with an error"
    grep -i "error" compile.log | head -3
    fail=$((fail + 1))
  elif ! "$LLVM_AS" output.ll -o /dev/null 2> check.log ||
       ! "$OPT" -passes=verify output.ll -o /dev/null 2>> check.log; then
    echo "FAIL $name: invalid IR"
    head -3 check.log
    fail=$((fail + 1))
  elif ! "$LINK_CXX" -w "$ROOT/$dir/driver.cpp" output.ll -o prog 2> link.log; then
    echo "FAIL $name: could not link with the driver"
    head -3 link.log
    fail=$((fail + 1))
  elif ! ./prog > run.log 2>&1; then
    echo "FAIL $name:"
    grep "FAILED" run.log
    fail=$((fail + 1))
  else
    echo "PASS $name ($(grep -c PASSED run.log) checks)"
    pass=$((pass + 1))
  fi
  cd "$ROOT" || exit 1
done

echo
echo "***** Programs that must be rejected *****"
for file in mytests/errors/*.c; do
  name=$(basename "$file" .c)
  expect=$(head -1 "$file" | sed 's#^// EXPECT: ##')
  rm -f "$WORK"/*
  cd "$WORK" || exit 1

  "$ROOT/mccomp" "$ROOT/$file" > compile.log 2>&1
  status=$?
  if [ $status -eq 0 ]; then
    echo "FAIL $name: accepted, but should be rejected ($expect)"
    fail=$((fail + 1))
  elif ! grep -qi -- "$expect" compile.log; then
    echo "FAIL $name: exit $status, but the message does not mention '$expect':"
    grep -i "error" compile.log | head -2
    fail=$((fail + 1))
  else
    echo "PASS $name: $(grep -i "error" compile.log | head -1)"
    pass=$((pass + 1))
  fi
  cd "$ROOT" || exit 1
done

echo
echo "passed=$pass failed=$fail"
if [ $fail -eq 0 ]; then
  echo "ALL MY TESTS PASSED"
  exit 0
fi
exit 1
