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
int global_zero(); void fill_table(); int table_sum(); float set_matrix();
int shadow_global(); int global_after_shadow();
extern int table[10];
extern float matrix[2][3];
}

int main() {
  check("global array starts at 0", global_zero() == 0);
  fill_table();
  check("1 + 2 + ... + 10 == 55", table_sum() == 55);
  check("driver sees the same global array", table[4] == 5);
  table[0] = 50;
  check("MiniC sees the driver's write", table_sum() == 104);
  check("2D global float array", close_to(set_matrix(), 5.0f));
  check("driver reads matrix[1][2] in row-major order", close_to(matrix[1][2], 2.5f));
  check("local array hides the global", shadow_global() == 99);
  check("global unchanged by the local", global_after_shadow() == 50);
  return failures;
}
