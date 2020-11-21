#include "lib.h"

int counter = 0;
jmp_buf buf;

void setjmp_longjmp_in_function() {
    int x = 1;
    setjmp(buf);  // set the jump position using buf
    printf("5");  // Prints a number
    x++;
    if (x <= 10) longjmp(buf, 1);  // Jump to the point located by setjmp
}

void call_setjmp() {
    setjmp(buf);  // set the jump position using buf
    printf("w");  // Prints a number
    counter++;
}
