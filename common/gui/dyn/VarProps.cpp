#include "VarProps.h"
#include <gui/dyn/ParseText.h>

namespace cmn::gui::dyn {


std::string PreVarProps::toStr() const {
    return "PreVarProps<"+std::string(name)+" parm="+Meta::toStr(parameters)+" subs="+Meta::toStr(subs)+">";
}

std::string VarProps::toStr() const {
    return "VarProps<"+std::string(name)+" parm="+Meta::toStr(parameters)+" subs="+Meta::toStr(subs)+">";
}

// Copy constructor
PreVarProps::PreVarProps(const PreVarProps& other)
    : original(other.original), // copy the string
      optional(other.optional),
      html(other.html)
{
    // Compute the offset in the original string for 'name'
    auto offset_name = other.name.data() - other.original.data();
    name = std::string_view(original.data() + offset_name, other.name.size());

    // Compute the offsets in the original string for 'subs' and 'parameters'
    subs.resize(other.subs.size());
    parameters.resize(other.parameters.size());
    for (size_t i = 0; i < other.subs.size(); ++i) {
        auto offset = other.subs[i].data() - other.original.data();
        subs[i] = std::string_view(original.data() + offset, other.subs[i].size());
    }
    for (size_t i = 0; i < other.parameters.size(); ++i) {
        auto offset = other.parameters[i].data() - other.original.data();
        parameters[i] = std::string_view(original.data() + offset, other.parameters[i].size());
    }
}

// Move constructor
PreVarProps::PreVarProps(PreVarProps&& other) noexcept {
    original = (other.original);
    optional = other.optional;
    html = other.html;
    
    // Compute the offset in the original string for 'name'
    auto offset_name = other.name.data() - other.original.data();
    name = std::string_view(original.data() + offset_name, other.name.size());

    // Compute the offsets in the original string for 'subs' and 'parameters'
    subs.resize(other.subs.size());
    parameters.resize(other.parameters.size());
    for (size_t i = 0; i < other.subs.size(); ++i) {
        auto offset = other.subs[i].data() - other.original.data();
        subs[i] = std::string_view(original.data() + offset, other.subs[i].size());
    }
    for (size_t i = 0; i < other.parameters.size(); ++i) {
        auto offset = other.parameters[i].data() - other.original.data();
        parameters[i] = std::string_view(original.data() + offset, other.parameters[i].size());
    }
}

VarProps PreVarProps::parse(const Context& context, State& state) const {
    VarProps props{
        .name = std::string(name),
        .optional = optional,
        .html = html
    };
    
    props.subs.reserve(subs.size());
    for(auto &s : subs)
        props.subs.emplace_back(s);
    
    if(props.name == "if" || props.name == "for")  {
        for(auto &p : parameters)
            props.parameters.emplace_back(p);
        
    } else {
        props.parameters.reserve(parameters.size());
        for(auto &p : parameters) {
            props.parameters.emplace_back(parse_text(p, context, state));
        }
    }
    
    return props;
}


}
