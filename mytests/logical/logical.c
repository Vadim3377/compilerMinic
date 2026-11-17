// Short-circuit && and ||, nested logical operators, non-bool operands.
int calls;

bool touch(bool v) { calls = calls + 1; return v; }
int count_calls() {
  bool r; int z;
  calls = 0; z = 0;
  r = touch(false) && touch(true);
  r = touch(true) || touch(false);
  r = touch(true) && touch(true);
  r = touch(false) || touch(false);
  if (z != 0 && 10 / z > 1) { calls = 100; }
  if (touch(false) || touch(true) && touch(false)) { calls = 200; }
  return calls;
}
bool and_or(bool a, bool b, bool c) { return a && (b || c); }
bool or_and(bool a, bool b, bool c) { return a || b && c; }
bool mixed_types(int a, float b) { return a && b; }
bool not_and(bool a, bool b) { return !(a && b); }
