#include "registry.h"

struct Interface {
    virtual ~Interface() {}
    virtual int get() const { return 0; }
};

struct InterfaceArgs {};

Registry<std::string, std::unique_ptr<Interface>, InterfaceArgs>* RegistryX();
