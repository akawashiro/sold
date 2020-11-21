#include "lib.h"

int main() {
    printf("call setjmp_longjmp_in_function\n");
    setjmp_longjmp_in_function();

    printf("\n\ncall setjmp\n");
    call_setjmp();
    if (counter <= 10) longjmp(buf, 1);  // Jump to the point located by setjmp
    return 0;
}
