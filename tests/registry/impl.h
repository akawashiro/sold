#include "interface.h"

struct Impl : public Interface {
    Impl(InterfaceArgs) {}
    int get() const override { return 133; }
};
