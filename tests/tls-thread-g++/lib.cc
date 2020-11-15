#include "lib.h"

int create_id() {
    thread_local int current_id = 0;
    return current_id++;
}

