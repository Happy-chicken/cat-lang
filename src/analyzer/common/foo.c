int f1(int a, int b, int c) {
    return a + b + c;
}

int f2(int a, int b) {
    return f1(a, b, 0);
}