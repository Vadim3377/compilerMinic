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
bool feq(float, float); bool fne_int(float, int); bool nan_ne(); bool nan_eq();
int nan_cond(); float average(int, int);
}

int main() {
  check("1.5 == 1.5", feq(1.5f, 1.5f) && !feq(1.5f, 2.5f));
  check("2.0 != 2 is false, 2.5 != 2 is true", !fne_int(2.0f, 2) && fne_int(2.5f, 2));
  check("NaN != NaN is true", nan_ne());
  check("NaN == NaN is false", !nan_eq());
  check("if (NaN) is taken, like C", nan_cond() == 1);
  check("(3 + 4) / 2.0 == 3.5", close_to(average(3, 4), 3.5f));
  return failures;
}
