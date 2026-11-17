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
int count_calls(); bool and_or(bool, bool, bool); bool or_and(bool, bool, bool);
bool mixed_types(int, float); bool not_and(bool, bool);
}

int main() {
  check("touch() called only when needed: 1 + 1 + 2 + 2 + 3 == 9", count_calls() == 9);
  int ok_and_or = 1;
  int ok_or_and = 1;
  for (int i = 0; i < 8; i++) {
    bool a = i & 4;
    bool b = i & 2;
    bool c = i & 1;
    if (and_or(a, b, c) != (a && (b || c)))
      ok_and_or = 0;
    if (or_and(a, b, c) != (a || (b && c)))
      ok_or_and = 0;
  }
  check("a && (b || c) for all 8 inputs", ok_and_or);
  check("a || b && c (&& binds tighter) for all 8 inputs", ok_or_and);
  check("int && float operands", mixed_types(2, 0.5f) && !mixed_types(0, 0.5f) && !mixed_types(2, 0.0f));
  check("!(a && b)", not_and(true, false) && !not_and(true, true));
  return failures;
}
