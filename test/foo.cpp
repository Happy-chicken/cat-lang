#include <cstdio>

int f1(int a, int b) {
  return a - b;
}

int f2(int x, int y) {
  int t = f1(x, y);
  return x + y + t;
}

int main() {
  int temp = 3;
  int temp2 = temp + 4;

  // int res = f2(1, 2);
  // printf("In main:%d", res);
  return 0;
}
