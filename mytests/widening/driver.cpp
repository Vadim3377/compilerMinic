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
float widen_assign(); int bool_plus(); int bool_neg(); int bool_compare();
float mixed(int, float); float arg_widen(); float chain_types(); int chain_same();
float pass_bool(); int not_int(int); float return_widened();
}

int main() {
  check("int, bool widened into float: 6 + 1 == 7.0", close_to(widen_assign(), 7.0f));
  check("true + true == 2", bool_plus() == 2);
  check("-true == -1", bool_neg() == -1);
  check("true > false", bool_compare() == 1);
  check("7 / 2 + 1.5 * 2 == 6.0 (int division first)", close_to(mixed(7, 1.5f), 6.0f));
  check("mixed(true, 2) == 4.0", close_to(arg_widen(), 4.0f));
  check("f = i = 3 then f / 2 == 1.5", close_to(chain_types(), 1.5f));
  check("a = b = 7", chain_same() == 14);
  check("bool argument to float parameter == 1.0", close_to(pass_bool(), 1.0f));
  check("!0 == 1 and !5 == 0", not_int(0) == 1 && not_int(5) == 0);
  check("return 3 from float function == 3.0", close_to(return_widened(), 3.0f));
  return failures;
}
