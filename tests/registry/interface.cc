#include "interface.h"

Registry<std::string, std::unique_ptr<Interface>, InterfaceArgs>* RegistryX() {
    static Registry<std::string, std::unique_ptr<Interface>, InterfaceArgs>* registry =
        new Registry<std::string, std::unique_ptr<Interface>, InterfaceArgs>();
    printf("%s:%d registry=%p\n", __FILE__, __LINE__, registry);
    return registry;
}
