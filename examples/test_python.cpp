#include <pybind11/embed.h>
#include <iostream>
namespace py = pybind11;

int main() {
    auto str = [](const wchar_t* input) {
        return input ? std::wstring(input) : std::wstring();
    };
    std::wcout << str(Py_GetPythonHome()) << std::endl;
    std::cout << Py_GetVersion() << std::endl;
    std::wcout << str(Py_GetPath()) << std::endl;
    std::wcout << Py_GetPlatform() << std::endl;
    std::wcout << str(Py_GetExecPrefix()) << std::endl;
    
    py::scoped_interpreter guard{};
    try {
        py::exec(R"(
            kwargs = dict(name="World", number=42)
            message = "Hello, {name}! The answer is {number}".format(**kwargs)
            print(message)
            import numpy as np
            import tensorflow as tf
            print(tf)
        )");
    }
    catch (py::error_already_set& e) {
        std::cout << "Error already set: " << e.what() << std::endl;
        e.restore();
    }
}
