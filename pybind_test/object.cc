#include <pybind11/pybind11.h>

namespace py = pybind11;

struct Pet {
    enum Kind { Dog = 0, Cat };

    Pet(const std::string& name) : name(name), type(Kind::Dog) {}
    Pet(const std::string& name, Kind type) : name(name), type(type) {}

    void set(int age_) { age = age_; }
    void set(const std::string& name_) { name = name_; }
    void setName(const std::string& name_) { name = name_; }
    const std::string& getName() const { return name; }
    const int getAge() const { return age; }

    int age;
    std::string name;
    Kind type;
};

struct Dog : Pet {
    Dog(const std::string& name) : Pet(name) {}
    std::string bark() const { return "woof!"; }
};

struct PolymorphicPet {
    virtual ~PolymorphicPet() = default;
};

struct PolymorphicDog : PolymorphicPet {
    std::string bark() const { return "woof!"; }
};

PYBIND11_MODULE(myobject, m) {
    py::class_<Pet> pet(m, "Pet");

    pet.def(py::init<const std::string&>())
        .def(py::init<const std::string&, Pet::Kind>())
        .def("set", py::overload_cast<int>(&Pet::set), "Set the pet's age")
        .def("set", py::overload_cast<const std::string&>(&Pet::set), "Set the pet's name")
        .def("setName", &Pet::setName)
        .def("getName", &Pet::getName)
        .def("getAge", &Pet::getAge)
        .def("__repr__", [](const Pet& a) { return "<example.Pet named '" + a.name + "'>"; })
        .def_readwrite("type", &Pet::type);
    py::enum_<Pet::Kind>(pet, "Kind").value("Dog", Pet::Kind::Dog).value("Cat", Pet::Kind::Cat).export_values();

    py::class_<Dog, Pet>(m, "Dog").def(py::init<const std::string&>()).def("bark", &Dog::bark);

    m.def("pet_store", []() { return std::unique_ptr<Pet>(new Dog("Molly")); });

    py::class_<PolymorphicPet>(m, "PolymorphicPet");
    py::class_<PolymorphicDog, PolymorphicPet>(m, "PolymorphicDog").def(py::init<>()).def("bark", &PolymorphicDog::bark);

    m.def("pet_store2", []() { return std::unique_ptr<PolymorphicPet>(new PolymorphicDog); });
}
