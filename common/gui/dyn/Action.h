#pragma once
#include <commons.pc.h>

namespace gui::dyn {

struct Context;
struct State;

struct Action {
    std::string name;
    std::vector<std::string> parameters;
    
    const std::string& first() const { return parameters.front(); }
    const std::string& last() const { return parameters.back(); }
    std::string toStr() const;
    static std::string class_name() { return "Action"; }
};

struct PreAction {
    std::string original;
    std::string_view name;
    std::vector<std::string_view> parameters;
    
    PreAction() noexcept = default;

    // Copy constructor
    PreAction(const PreAction& other)
        : original(other.original)
    {
        name = std::string_view(original.data() + (other.name.data() - other.original.data()), other.name.size());
        parameters.reserve(other.parameters.size());
        for (const auto& param : other.parameters) {
            auto offset = param.data() - other.original.data();
            parameters.emplace_back(original.data() + offset, param.size());
        }
    }

    // Move constructor
    PreAction(PreAction&& other) noexcept {
        original = (other.original);
        name = std::string_view(original.data() + (other.name.data() - other.original.data()), other.name.size());
        parameters.reserve(other.parameters.size());
        for (const auto& param : other.parameters) {
            auto offset = param.data() - other.original.data();
            parameters.emplace_back(original.data() + offset, param.size());
        }
    }

    // Copy assignment operator
    PreAction& operator=(const PreAction& other) = delete;

    // Move assignment operator
    PreAction& operator=(PreAction&& other) noexcept = default;
    
    static PreAction fromStr(std::string_view);
    std::string toStr() const;
    static std::string class_name() { return "PreAction"; }
    
    Action parse(const Context& context, State& state) const;
};

}
