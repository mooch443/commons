#include <pybind11/embed.h>
namespace py = pybind11;

int main() {
    

    std::cout << Py_GetPythonHome() << std::endl;
    std::cout << Py_GetVersion() << std::endl;
    std::cout << Py_GetPath() << std::endl;
    std::cout << Py_GetPlatform() << std::endl;
    std::cout << Py_GetExecPrefix() << std::endl;
    
    py::scoped_interpreter guard{};
    py::exec(R"(
        kwargs = dict(name="World", number=42)
        message = "Hello, {name}! The answer is {number}".format(**kwargs)
        print(message)
    )");
}
