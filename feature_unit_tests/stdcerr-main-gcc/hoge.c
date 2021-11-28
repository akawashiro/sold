#include <stdio.h>

void hello_from_hoge() {
    fprintf(stderr, "%s:%d hello from stderr\n", __FILE__, __LINE__);
}
