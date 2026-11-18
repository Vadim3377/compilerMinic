// EXPECT: invalid array use
int f(int a[3]) { return a[0]; } int main() { float b[3]; return f(b); }
