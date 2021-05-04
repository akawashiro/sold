#include <pybind11/pybind11.h>

namespace py = pybind11;
using namespace pybind11::literals;

int add(int i, int j) {
    return i + j;
}

PYBIND11_MODULE(myadd, m) {
    m.doc() = "pybind11 example plugin";  // optional module docstring
    m.def("myadd", &add, "A function which adds two numbers");
    m.def("myadd2", &add, "i"_a, "j"_a);
    m.def("myadd3", &add, "i"_a = 1, "j"_a = 2);

    m.attr("the_answer") = 42;
    py::object world = py::cast("World");
    m.attr("what") = world;
}
