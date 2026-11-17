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
int read_untouched(); void increment(); int read_counter(); float use_globals();
extern int counter;
}

int main() {
  check("global starts at 0", read_untouched() == 0);
  increment();
  increment();
  check("global keeps its value between calls", read_counter() == 2);
  check("driver sees the same global", counter == 2);
  counter = 40;
  check("MiniC sees a write from the driver", read_counter() == 40);
  check("float and bool globals: 2 * 2.5 == 5.0", close_to(use_globals(), 5.0f));
  return failures;
}
