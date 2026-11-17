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
int shadow(int); int get_x(); int param_shadow(int); int param_assign(int); int factorial(int);
}

int main() {
  check("inner x hides outer and global: 1 + 10 + 1 + 5 == 17", shadow(5) == 17);
  check("global x is still 1 after shadow()", get_x() == 1);
  check("inner a does not change parameter a", param_shadow(9) == 9);
  check("assigning to a parameter", param_assign(21) == 42);
  check("local named like its function", factorial(5) == 120);
  return failures;
}
