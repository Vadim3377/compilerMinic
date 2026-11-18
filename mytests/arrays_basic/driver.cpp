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
int print_int(int X) { printf("%d\n", X); return 0; }
int zero_init(); int fill_and_sum(int); int expr_index(); int element_as_arg();
float float_array(); int nested_index(); int bool_index(); int chain_assign();
}

int main() {
  check("local array starts at 0", zero_init() == 0);
  check("fill then sum: 0+1+4+...+81 == 285", fill_and_sum(10) == 285);
  check("expression indices arr[i+1], arr[(i+1)*(j-2)+f(1)]", expr_index() == 59);
  check("element as call argument", element_as_arg() == 42);
  check("float array with widened int store: 3.5 + 0.0", close_to(float_array(), 3.5f));
  check("a[a[a[0]]] == 7", nested_index() == 7);
  check("bool index widened to int", bool_index() == 6);
  check("x = a[0] = a[1] = 8", chain_assign() == 24);
  return failures;
}
