#include <c10/core/DeviceType.h>

#include <iostream>
#include <string>

int main() {
    std::string str = c10::DeviceTypeName(c10::DeviceType::CPU);
    std::cout << str << std::endl;
    return 0;
}

