int hoge_var = 0xdeadbeef;

void inc() {
    hoge_var++;
    printf("%s:%d hoge_var = %x &hoge_var = %x\n", __FILE__, __LINE__, hoge_var, &hoge_var);
}
