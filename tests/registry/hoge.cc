#include "interface.h"

void hoge() {
    auto r = RegistryX();
    auto i = r->Create("impl", InterfaceArgs{});
    printf("%d\n", i.release()->get());
}
