#include "hoge.h"

int main() {
    printf("call setjmp_longjmp_in_function\n");
    setjmp_longjmp_in_function();

    printf("main: &counter = %p &buf = %p\n", &counter, &buf);
    printf("call longjmp\n");
    counter = 0;

    setjmp(buf);                        // set the jump position using buf
    printf("counter = %d\n", counter);  // Prints a number
    counter++;
    call_longjmp();
    return 0;
}
