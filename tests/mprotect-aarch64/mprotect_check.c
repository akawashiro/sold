#include <stdio.h>

#include "base.h"

void check() {
    int x = func(1, 2);
    char str[1024];
    FILE* fp = fopen("/proc/self/maps", "r");
    while ((fgets(str, 256, fp)) != NULL) {
        printf("%s", str);
    }
    fclose(fp);
}
