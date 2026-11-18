// EXPECT: invalid array use
int f(int m[2][4]) { return 0; } int main() { int m[2][5]; return f(m); }
