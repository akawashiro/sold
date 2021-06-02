#include <iostream>
#include <mutex>
#include <thread>

std::once_flag once;

void init() {
    std::cout << "initialize" << std::endl;
}

void thread_proc() {
    std::call_once(once, init);
}
