extern thread_local int dummy;
extern thread_local int fuga;
thread_local int hoge;

void fn() {
    dummy = 10;
    fuga = 10;
}
