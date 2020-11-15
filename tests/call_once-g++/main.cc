#include <iostream>

#include "lib.h"

int main() {
    std::thread t1(thread_proc);
    std::thread t2(thread_proc);
    std::thread t3(thread_proc);

    t1.join();
    t2.join();
    t3.join();
}
