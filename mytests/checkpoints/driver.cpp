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
int five(); float half(); bool yes(); void nothing();
int scopes(); int arith(); int calls(); int loop();
int shortcircuit(); int sideeffects();
}

int main() {
  nothing();
  check("cp2 five() == 5", five() == 5);
  check("cp2 half() == 0.5", close_to(half(), 0.5f));
  check("cp2 yes() == true", yes() == true);
  check("cp3 shadowing keeps outer y == 7", scopes() == 7);
  check("cp4 int division then widening == 31", arith() == 31);
  check("cp5 calls with widened args == 26", calls() == 26);
  check("cp6 while + if/else == 35", loop() == 35);
  check("cp7 && / || skip division by zero == 2", shortcircuit() == 2);
  check("cp8 right operand only when needed == 1", sideeffects() == 1);
  return failures;
}
