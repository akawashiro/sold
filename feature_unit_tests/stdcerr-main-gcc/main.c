#include <stdio.h>

void hello_from_hoge();

int main() {
    fprintf(stderr, "%s:%d hello from stderr\n", __FILE__, __LINE__);
    hello_from_hoge();
    return 0;
}
