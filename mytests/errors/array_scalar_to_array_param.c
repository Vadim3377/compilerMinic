// EXPECT: invalid array use
int f(int a[3]) { return a[0]; } int main() { int x; x = 1; return f(x); }
