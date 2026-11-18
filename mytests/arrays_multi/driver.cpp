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
int grid(); float cube(); int index_expr(); int fresh_each_iteration(); int diag(int);
}

int main() {
  check("g[i][j] = i*10+j: 0 + 12 + 23 == 35", grid() == 35);
  check("3D element with expression index: 1.5 * 2 == 3.0", close_to(cube(), 3.0f));
  check("m[i+2][j*2]", index_expr() == 7);
  check("array declared in a loop body is zeroed each time", fresh_each_iteration() == 0);
  check("only the diagonal is set: 3 * 5 == 15", diag(5) == 15);
  return failures;
}
