#pragma once
#include <commons.pc.h>

namespace cmn::gui::dyn {

struct Context;
struct State;

#define REQUIRE_EXACTLY(N, props) ::cmn::gui::dyn::require_exactly< N >(props, source_location::current())
template<size_t N>
void require_exactly(auto const & props, source_location loc) {
    if(props.parameters.size() != N) {
        throw InvalidArgumentException("Invalid number of arguments for ", props, " (expected ", N, ") @ ", loc.file_name(),"::", loc.function_name(), ":", loc.line());
    }
}

#define REQUIRE_AT_LEAST(N, props) ::cmn::gui::dyn::require_at_least< N >(props, source_location::current())
template<size_t N>
void require_at_least(auto const & props, source_location loc) {
    static_assert(N > 0, "We cannot have fewer than zero arguments.");
    if(props.parameters.size() < N) {
        throw InvalidArgumentException("Not enough arguments for ", props, " (need at least ", N, ") @ ", loc.file_name(),"::", loc.function_name(), ":", loc.line());
    }
}

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
