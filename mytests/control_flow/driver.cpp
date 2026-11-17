#include <cstdio>
#include <cmath>

static int failures = 0;

// Prints one line per check, in the same style as the coursework drivers.
static void check(const char *what, bool ok) {
  if (ok) {
    printf("PASSED %s\n", what);
  } else {
    printf("FAILED %s\n", what);
    failures++;
  }
}

static bool close_to(float a, float b) { return std::fabs(a - b) < 1e-5f; }

extern "C" {
int cond_int(int); int both_return(int); int after_return(int); int return_in_loop(int);
int falls_off(int); int empty_int(); int empty_while(); int if_no_else(int);
int nested_if(int, int); int nested_loops(int); int call_void();
}

int main() {
  check("while (n) counts down, if (2.5) is true: 3 + 100", cond_int(3) == 103);
  check("return in both branches", both_return(1) == 1 && both_return(-1) == 2);
  check("code after return is skipped", after_return(4) == 4);
  check("return from inside while (true)", return_in_loop(2) == 6);
  check("int function without return gives 0", falls_off(1) == 0 && empty_int() == 0);
  check("while (step()) ; with empty body runs 5 times", empty_while() == 5);
  check("if without else", if_no_else(3) == 1 && if_no_else(4) == 0);
  check("nested if/else, all four paths",
        nested_if(1, 1) == 1 && nested_if(1, 0) == 2 && nested_if(0, 1) == 3 && nested_if(0, 0) == 4);
  check("nested while loops: 0+1+2+3 == 6", nested_loops(4) == 6);
  check("void calls and early return in void function", call_void() == 77);
  return failures;
}
