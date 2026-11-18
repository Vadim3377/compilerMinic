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

extern "C" { void init(); void multiply(); float get_c(int, int); }

int main() {
  init();
  multiply();
  int ok = 1;
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++) {
      float expect = 0;
      for (int k = 0; k < 3; k++)
        expect += (float)(i + k) * (float)(k - j);
      if (!close_to(get_c(i, j), expect))
        ok = 0;
    }
  check("c = a * b matches the C++ result for all 9 entries", ok);
  return failures;
}
