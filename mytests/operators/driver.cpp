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
int precedence(); int left_assoc(); int div_assoc(); int signed_div(); int signed_mod();
float float_neg(float); int double_neg(int); int compare_all(int, int);
int parens(); int unary_prec();
}

int main() {
  check("2 + 3 * 4 - 10 / 2 % 3 == 12", precedence() == 12);
  check("20 - 5 - 3 == 12 (left associative)", left_assoc() == 12);
  check("64 / 4 / 2 == 8", div_assoc() == 8);
  check("-7 / 2 == -3 (truncates like C)", signed_div() == -3);
  check("-7 % 3 == -1 (srem, like C)", signed_mod() == -1);
  check("-(2.5) == -2.5", close_to(float_neg(2.5f), -2.5f));
  check("- -4 == 4", double_neg(4) == 4);
  check("1 vs 2: < <= != set", compare_all(1, 2) == 1 + 2 + 32);
  check("2 vs 2: <= >= == set", compare_all(2, 2) == 2 + 8 + 16);
  check("3 vs 2: > >= != set", compare_all(3, 2) == 4 + 8 + 32);
  check("(2 + 3) * (4 - 1) == 15", parens() == 15);
  check("-2 * 3 + !0 == -5", unary_prec() == -5);
  return failures;
}
