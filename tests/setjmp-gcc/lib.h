#include <setjmp.h>
#include <stdio.h>

extern int counter;
extern jmp_buf buf;

void setjmp_longjmp_in_function();
void call_setjmp();
