#pragma once

#include <commons.pc.h>
#include <gui/DynamicVariable.h>

namespace cmn::sprite {
class Reference;
}

namespace gui::dyn {

struct Context;
struct State;

struct VarProps {
    std::string name;
    std::vector<std::string> subs;
    std::vector<std::string> parameters;
    bool optional{false};
    bool html{false};
    
    const std::string& first() const { return parameters.front(); }
    const std::string& last() const { return parameters.back(); }
    
    static VarProps fromStr(std::string);
    std::string toStr() const;
    static std::string class_name() { return "VarProps"; }
    //operator std::string() const { return last(); }
};

struct PreVarProps {
    std::string original;
    std::string_view name;
    std::vector<std::string_view> subs;
    std::vector<std::string_view> parameters;
    
    bool optional{false};
    bool html{false};
    
    PreVarProps() noexcept = default;
    
    // Copy constructor
    PreVarProps(const PreVarProps& other);
    
    // Move constructor
    PreVarProps(PreVarProps&& other) noexcept;
    
    std::string toStr() const;
    static std::string class_name() { return "PreVarProps"; }
    VarProps parse(const Context& context, State& state) const;
};

using VarBase_t = VarBase<const VarProps&>;
using VarReturn_t = cmn::sprite::Reference;

}
