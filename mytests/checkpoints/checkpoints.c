// The checkpoints from the Part 2 tutorial (Steps 2 to 8), as functions the
// driver can call instead of separate main() programs.
int counter;

int five() { return 5; }
float half() { return 0.5; }
bool yes() { return true; }
void nothing() { }

int scopes() { int x; int y; x = 7; y = x; { int x; x = 1; } return y; }

int arith() {
  int a; float f;
  a = 7;
  f = a / 2 + 0.5;
  return (f == 3.5) + (a % 4) * 10;
}

int square(int n) { return n * n; }
float halfof(float x) { return x / 2; }
int calls() { return square(5) + (halfof(3) == 1.5); }

int loop() {
  int i; int s;
  i = 1; s = 0;
  while (i <= 10) {
    if (i % 2 == 0) { s = s + i; } else { s = s + 1; }
    i = i + 1;
  }
  return s;
}

int shortcircuit() {
  int z;
  z = 0;
  if (z != 0 && 10 / z > 1) { return 1; }
  if (z == 0 || 10 / z > 1) { return 2; }
  return 3;
}

bool bump() { counter = counter + 1; return true; }
int sideeffects() {
  bool r;
  counter = 0;
  r = false && bump();
  r = true || bump();
  r = true && bump();
  return counter;
}
