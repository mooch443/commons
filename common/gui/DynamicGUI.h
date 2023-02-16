#pragma once

#include <commons.pc.h>
#include <JSON.h>
#include <gui/colors.h>
#include <file/Path.h>
#include <gui/types/Layout.h>
#include <misc/PackLambda.h>
#include <misc/SpriteMap.h>
#include <misc/Timer.h>

namespace gui {
namespace dyn {

template <typename T> constexpr std::string_view type_name();

template <>
constexpr std::string_view type_name<void>()
{ return "void"; }

namespace detail {

using type_name_prober = void;

template <typename T>
constexpr std::string_view wrapped_type_name()
{
#ifdef __clang__
    return __PRETTY_FUNCTION__;
#elif defined(__GNUC__)
    return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
    return __FUNCSIG__;
#else
#error "Unsupported compiler"
#endif
}

constexpr std::size_t wrapped_type_name_prefix_length() {
    return wrapped_type_name<type_name_prober>().find(type_name<type_name_prober>());
}

constexpr std::size_t wrapped_type_name_suffix_length() {
    return wrapped_type_name<type_name_prober>().length()
        - wrapped_type_name_prefix_length()
        - type_name<type_name_prober>().length();
}

} // namespace detail

template <typename T>
constexpr std::string_view type_name() {
    constexpr auto wrapped_name = detail::wrapped_type_name<T>();
    constexpr auto prefix_length = detail::wrapped_type_name_prefix_length();
    constexpr auto suffix_length = detail::wrapped_type_name_suffix_length();
    constexpr auto type_name_length = wrapped_name.length() - prefix_length - suffix_length;
    return wrapped_name.substr(prefix_length, type_name_length);
}

//template <typename T>
//struct Variable : Variable<decltype(&T::operator())>
//{ };

//template<class T> class Variable;
template<class return_type, typename... arg_types>
class Variable;

template<class F, typename R = typename cmn::detail::return_type<F>::type, typename arg = typename  cmn::detail::arg_type<F>::type>
Variable(F&&) -> Variable<R, arg>;

template<typename... _Args>
class VarBase;

template <typename T>
struct arg_types : arg_types<decltype(&T::operator())>
{ };

template <typename ClassType, typename ReturnType, typename Arg, typename... Args>
struct arg_types<ReturnType(ClassType::*)(Arg, Args...) const>
{
    using type = Arg;
};

template <typename ClassType, typename ReturnType>
struct arg_types<ReturnType(ClassType::*)() const>
{
    using type = void;
};

template <typename T, typename R>
struct function_type : function_type<R, decltype(&T::operator())>
{ };

template <typename ClassType, typename R, typename ReturnType, typename... Args>
struct function_type<R, ReturnType(ClassType::*)(Args...) const>
{
    using type = std::function<R(Args...)>;
};


template<class return_type, typename... arg_types>
class Variable : public VarBase<arg_types...> {
public:
    //using return_type = typename cmn::detail::return_type<F>::type;//typename std::invoke_result_t<F>;
    //using arg_types = typename dyn::arg_types<F>::type;

private:
    static constexpr auto refcode = type_name<return_type>();

protected:
    std::function<return_type(arg_types...)> function;
    
public:
    using VarBase<arg_types...>::value_string;
    
public:
    constexpr Variable(std::function<return_type(arg_types...)> fn) noexcept
        :   VarBase<arg_types...>(refcode),
            function(std::move(fn))
    {
        value_string = [this](auto... args) -> std::string {
            if constexpr(std::same_as<std::string, return_type>)
                return function(std::forward<decltype(args)>(args)...);
            else if constexpr(_clean_same<sprite::Map&, return_type>) {
                auto& map = function(args...);
                
                auto access = [&map](std::string key) -> std::string {
                    auto ref = map[key];
                    if(ref.get().valid()) {
                        if(ref.template is_type<std::string>()) {
                            auto str = ref.get().valueString();
                            if(str.length() >= 2)
                                return str.substr(1, str.length()-2);
                        }
                        return ref.get().valueString();
                    }
                    throw std::invalid_argument("Cannot find given parameter.");
                };
                
                return ( access(args), ... );
                
            }
            else if constexpr(_clean_same<sprite::ConstReference, return_type> || _clean_same<sprite::Reference, return_type>) {
                auto r = function(std::forward<decltype(args)>(args)...);
                if(r.get().valid()) {
                    if constexpr(_clean_same<std::string, return_type>) {
                        auto str = r.get().valueString();
                        print("IS STRING: ", str);
                        return str.length() < 2 ? "" : str.substr(1, str.length()-2);
                    }
                    return r.get().valueString();
                } else
                    return "null";
            } else
                return Meta::toStr( function(std::forward<decltype(args)>(args)...) );
        };
    }

    template<typename... Args>
    constexpr return_type get(Args... args) const noexcept {
        return function(std::forward<Args>(args)...);
    }
};

template<typename... _Args>
class VarBase {
protected:
    const std::string_view type;
    
public:
    std::function<std::string(_Args...)> value_string;
    
public:
    VarBase(std::string_view type) : type(type) {}
    virtual ~VarBase() {}
    
    template<typename T>
    constexpr bool is() const noexcept {
        //return dynamic_cast<const Variable<T,  _Args...>*>(this) != nullptr;
        //
        
        return type == type_name<T>();
    }

    template<typename T>
    constexpr T value(_Args... args) const noexcept {
        using Target_t = const Variable<T, _Args...>*;
        static_assert(std::same_as<const VarBase<_Args...>*, decltype(this)>);
        
        assert(dynamic_cast<Target_t>(this) != nullptr);
        return static_cast<Target_t>(this)->get(std::forward<_Args>(args)...);
    }

    constexpr auto class_name() const { return type; }
};

ENUM_CLASS(LayoutType, each, condition, vlayout, hlayout, collection, button, text, stext, rect);

inline Color parse_color(const auto& obj) {
    if(not obj.is_array())
        throw std::invalid_argument("Color is not an array.");
    if(obj.size() < 3)
        throw std::invalid_argument("Color is not an array of RGB.");
    uint8_t a = 255;
    if(obj.size() >= 4) {
        a = obj[3].template get<uint8_t>();
    }
    return Color(obj[0].template get<uint8_t>(),
                 obj[1].template get<uint8_t>(),
                 obj[2].template get<uint8_t>(),
                 a);
}


using VarBase_t = VarBase<std::string>;
using VarReturn_t = sprite::Reference;

struct Context {
    std::unordered_map<std::string, std::function<void(Event)>> actions;
    std::unordered_map<std::string, std::shared_ptr<VarBase_t>> variables;
};

struct LoopBody;
struct IfBody;

struct State {
    size_t object_index{0};
    std::unordered_map<size_t, std::unordered_map<std::string, std::string>> patterns;
    std::unordered_map<size_t, std::function<void(DrawStructure&)>> display_fns;
    std::unordered_map<size_t, LoopBody> loops;
    std::unordered_map<size_t, IfBody> ifs;
};

namespace Modules {
struct Module {
    std::string _name;
    std::function<void(size_t, State&, const Layout::Ptr&)> _apply;
};

void add(Module&&);
Module* exists(const std::string& name);
}

struct LoopBody {
    std::string variable;
    nlohmann::json child;
    State state;
    std::vector<std::shared_ptr<VarBase_t>> cache;
};

struct IfBody {
    std::string variable;
    Layout::Ptr _if;
    Layout::Ptr _else;
};

Layout::Ptr parse_object(const nlohmann::json& obj,
                         const Context& context,
                         State& state);

std::string parse_text(const std::string& pattern, const Context& context);

template<typename ApplyF, typename ErrorF>
inline auto resolve_variable(const std::string& word, const Context& context, ApplyF&& apply, ErrorF&& error) -> typename cmn::detail::return_type<ApplyF>::type {
    auto parts = utils::split(word, ':');
    auto variable = utils::lowercase(parts.front());
    bool optional = false;
    if(not variable.empty() && variable.front() == '.') {
        // optional variable
        variable = variable.substr(1);
        optional = true;
    }
    
    std::string modifiers;
    if(parts.size() > 1)
        modifiers = parts.back();
    
    if(context.variables.contains(variable)) {
        if constexpr(std::invocable<ApplyF, VarBase_t&, const std::string&>)
            return apply(*context.variables.at(variable), modifiers);
        else if constexpr(std::invocable<ApplyF, VarBase_t&, const std::string&, bool>)
            return apply(*context.variables.at(variable), modifiers, optional);
        else
            static_assert(std::invocable<ApplyF, VarBase_t&, const std::string&, bool>);
    }
    
    if constexpr(std::invocable<ErrorF, bool>)
        return error(optional);
    else
        return error();
}

template<typename T>
T resolve_variable_type(std::string word, const Context& context) {
    if(word.empty())
        throw std::invalid_argument("Invalid variable type for "+word);
    
    if(word.length() > 2
       && word.front() == '{'
       && word.back() == '}') {
        word = word.substr(1,word.length()-2);
    }
    
    return resolve_variable(word, context, [&](const VarBase_t& variable, const std::string& modifiers) -> T {
        if(variable.is<T>()) {
            return variable.value<T>(modifiers);
            
        } else if(variable.is<sprite::Map&>()) {
            auto& tmp = variable.value<sprite::Map&>("null");
            auto ref = tmp[modifiers];
            if(ref.template is_type<T>()) {
                return ref.get().template value<T>();
            }
            
        } else if(variable.is<VarReturn_t>()) {
            auto tmp = variable.value<VarReturn_t>(modifiers);
            if(tmp.template is_type<T>()) {
                return tmp.get().template value<T>();
            }
        }
        
        throw std::invalid_argument("Invalid variable type for "+word);
        
    }, [&]() -> T {
        throw std::invalid_argument("Invalid variable type for "+word);
    });
}

void update_objects(DrawStructure&, const Layout::Ptr& o, const Context& context, State& state);

inline tl::expected<nlohmann::json, const char*> load(const file::Path& path){
    static Timer timer;
    if(timer.elapsed() < 0.15) {
        return tl::unexpected("Have to wait longer to reload.");
    }
    timer.reset();
    
    auto text = path.read_file();
    static std::string previous;
    if(previous != text) {
        previous = text;
        return nlohmann::json::parse(text)["objects"];
    } else
        return tl::unexpected("Nothing changed.");
}

void update_layout(const file::Path&, Context&, State&, std::vector<Layout::Ptr>&);

}
}
