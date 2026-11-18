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
int sum(int *, int); void set_all(int *, int, int); int pass_on(int *, int);
float trace(float (*)[3]); float local_calls(); int by_reference();
}

int main() {
  int data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  check("C++ array passed to MiniC sum()", sum(data, 10) == 55);
  set_all(data, 3, 7);
  check("MiniC writes into the C++ array", data[0] == 7 && data[2] == 7 && data[3] == 4);
  check("array parameter passed on to another function", pass_on(data, 4) == 25);
  float m[3][3] = {{1, 0, 0}, {0, 2, 0}, {0, 0, 3}};
  check("2D float array from C++", close_to(trace(m), 6.0f));
  check("local arrays, a row m[2] and a 2D array as arguments: 20 + 10 + 10 + 7",
        close_to(local_calls(), 47.0f));
  check("callee writes are visible to the caller", by_reference() == 42);
  return failures;
}
