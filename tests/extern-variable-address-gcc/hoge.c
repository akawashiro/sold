#include "hoge.h"

int counter = 0;
jmp_buf buf;

void setjmp_longjmp_in_function() {
    printf("hoge: &counter = %p &buf = %p\n", &counter, &buf);
    int x = 1;
    setjmp(buf);            // set the jump position using buf
    printf("x = %d\n", x);  // Prints a number
    x++;
    if (x <= 5) longjmp(buf, 1);  // Jump to the point located by setjmp
}

void call_longjmp() {
    if (counter <= 5) longjmp(buf, 1);  // Jump to the point located by setjmp
}
