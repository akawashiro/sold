#include <iostream>

#include "lib.h"

int main() {
    int t1_id1, t1_id2;
    std::thread t1([&t1_id1, &t1_id2] {
        t1_id1 = create_id();
        t1_id2 = create_id();
    });

    int t2_id1, t2_id2;
    std::thread t2([&t2_id1, &t2_id2] {
        t2_id1 = create_id();
        t2_id2 = create_id();
    });

    t1.join();
    t2.join();

    std::cout << "t1_id1=" << t1_id1 << " t1_id2=" << t1_id2 << " t2_id1=" << t2_id1 << " t2_id2=" << t2_id2 << std::endl;
}
