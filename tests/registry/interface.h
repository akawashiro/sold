#include "registry.h"

struct Interface {
    virtual ~Interface() {}
    virtual int get() const { return 0; }
};

struct InterfaceArgs {};

static Registry<std::string, std::unique_ptr<Interface>, InterfaceArgs>* RegistryX() {
    std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
    static Registry<std::string, std::unique_ptr<Interface>, InterfaceArgs>* registry =
        new Registry<std::string, std::unique_ptr<Interface>, InterfaceArgs>();
    return registry;
}
