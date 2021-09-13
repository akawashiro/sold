#include <assert.h>
#include <stdio.h>

// Although `fugafuga` in fugafuga.c is subtraction, `hoge` in hoge.c is
// addition. renamer renames `hoge` to `fugafuga`.
int fugafuga(int a, int b);

int main() {
    int a = fugafuga(1, 2);
    assert(a == 3);
    return 0;
}
