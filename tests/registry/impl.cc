#include "impl.h"

int dummy;
static Registerer<std::string, std::unique_ptr<Interface>, InterfaceArgs> RegistererX(
    "impl", RegistryPriority::REGISTRY_DEFAULT, RegistryX(),
    Registerer<std::string, std::unique_ptr<Interface>, InterfaceArgs>::DefaultCreator<Impl>, "help");
