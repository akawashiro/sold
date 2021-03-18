#include "interface.h"

void hoge() {
    std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    auto r = RegistryX();
    std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    auto i = r->Create("impl", InterfaceArgs{});
    std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    printf("%d\n", i->get());
}
