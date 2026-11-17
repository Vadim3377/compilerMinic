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
bool is_even(int); bool is_odd(int); int fib(int); bool less(int, int);
bool flip(bool); int bool_to_int(bool);
}

int main() {
  check("mutual recursion: is_even(10), is_odd(7)", is_even(10) && is_odd(7) && !is_even(7));
  check("fib(10) == 55", fib(10) == 55);
  check("bool return to C++", less(1, 2) == true && less(2, 1) == false);
  check("bool argument from C++", flip(true) == false && flip(false) == true);
  check("bool parameter widened to int", bool_to_int(true) == 1 && bool_to_int(false) == 0);
  return failures;
}
