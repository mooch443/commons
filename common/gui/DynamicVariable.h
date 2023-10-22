#pragma once
#include <commons.pc.h>

namespace gui {
namespace dyn {

/// @brief Template for compile-time type name extraction.
template <typename T> constexpr std::string_view type_name();

/// @brief Specialization of type_name for the 'void' type.
/// @return "void" as a compile-time string.
template <>
constexpr std::string_view type_name<void>()
{ return "void"; }

namespace detail {

/// @brief Dummy type used to probe the type-name layout from the compiler.
using type_name_prober = void;

/// @brief Internal utility to get the full signature of a function or callable type.
/// The exact output depends on the compiler being used.
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

/// @brief Computes the length of the prefix that precedes the actual type name.
/// For example, in GCC, "__PRETTY_FUNCTION__" might produce
/// "constexpr std::string_view detail::wrapped_type_name() [T = int]"
/// and the prefix is "constexpr std::string_view detail::wrapped_type_name() [T = ".
constexpr std::size_t wrapped_type_name_prefix_length() {
    return wrapped_type_name<type_name_prober>().find(type_name<type_name_prober>());
}

/// @brief Computes the length of the suffix that follows the actual type name.
/// For example, in GCC, "__PRETTY_FUNCTION__" might produce
/// "constexpr std::string_view detail::wrapped_type_name() [T = int]"
/// and the suffix is "]".
constexpr std::size_t wrapped_type_name_suffix_length() {
    return wrapped_type_name<type_name_prober>().length()
    - wrapped_type_name_prefix_length()
    - type_name<type_name_prober>().length();
}

} // namespace detail

/// @brief Main function to extract the type name.
/// This function removes the prefix and suffix from the compiler-specific full signature
/// to isolate and return the actual type name.
template <typename T>
constexpr std::string_view type_name() {
    constexpr auto wrapped_name = detail::wrapped_type_name<T>();
    constexpr auto prefix_length = detail::wrapped_type_name_prefix_length();
    constexpr auto suffix_length = detail::wrapped_type_name_suffix_length();
    constexpr auto type_name_length = wrapped_name.length() - prefix_length - suffix_length;
    return wrapped_name.substr(prefix_length, type_name_length);
}

/// @brief Class template to represent a general variable.
template<class return_type, typename... arg_types>
class Variable;

/// @brief Deduction guide for Variable.
/// This exists to allow users to instantiate a Variable object without explicitly
/// specifying the template arguments. The guide deduces 'return_type' and 'arg'
/// from a callable object 'F'.
template<class F, typename R = typename cmn::detail::return_type<F>::type, typename arg = typename  cmn::detail::arg_type<F>::type>
Variable(F&&) -> Variable<R, arg>;

/// @brief Base class template for Variable.
template<typename... _Args>
class VarBase;

/// @brief Template structure for extracting argument types from callable objects.
/// This serves as the general case which defers to the specialization.
template <typename T>
struct arg_types : arg_types<decltype(&T::operator())>
{ };

/// @brief Specialization for extracting argument types from callable objects.
/// Works by decltype on the 'operator()' method, which all callable objects must have.
template <typename ClassType, typename ReturnType, typename Arg, typename... Args>
struct arg_types<ReturnType(ClassType::*)(Arg, Args...) const>
{
    using type = Arg; // Store the first argument type.
};

/// @brief Specialization for callable objects with no arguments.
template <typename ClassType, typename ReturnType>
struct arg_types<ReturnType(ClassType::*)() const>
{
    using type = void; // Store 'void' if no arguments.
};

/// @brief Template structure for function type deduction.
/// This structure is used to transform a callable type into its std::function equivalent.
template <typename T, typename R>
struct function_type : function_type<R, decltype(&T::operator())>
{ };

/// @brief Specialization of function_type for class-type callables.
/// This provides the actual mapping between a callable type and its std::function representation.
template <typename ClassType, typename R, typename ReturnType, typename... Args>
struct function_type<R, ReturnType(ClassType::*)(Args...) const>
{
    using type = std::function<R(Args...)>;
};

/**
  Represents a typed variable that holds a function.
  The function encapsulates some computation or data retrieval.
*/
template<class return_type, typename... arg_types>
class Variable : public VarBase<arg_types...> {
private:
    /// Human-readable string representing the type of 'return_type'.
    static constexpr auto refcode = type_name<return_type>();
    
protected:
    /// Function that encapsulates the computation or data retrieval.
    std::function<return_type(arg_types...)> function;
    
public:
    using VarBase<arg_types...>::value_string;
    
public:
    /**
      Constructor that takes a function object and stores it.
      @param fn A std::function object that matches the return_type and arg_types.
    */
    constexpr Variable(std::function<return_type(arg_types...)> fn) noexcept
        :   VarBase<arg_types...>(refcode),
            function(std::move(fn))
    {
        // Lambda to convert the captured function's return value to a string
        value_string = [this](auto... args) -> std::string {
            if constexpr(_clean_same<std::string, return_type>) {
                // if we encounter a string type, we can already return without converting
                // or adding any quotes
                return function( std::forward<decltype(args)>(args)... );
                
            } else if constexpr(_clean_same<file::Path, return_type>) {
                return function( std::forward<decltype(args)>(args)... ).str();
                
            } else if constexpr(_clean_same<sprite::Map&, return_type>) {
                // for sprite::Maps we have to do some more work.
                // here we get an entire sprite::Map reference, instead of a single
                // constref from a map, so we get the map first and then
                // use another indirection to get the actual value as a string.
                // here, we could have multiple args theoretically.
                sprite::Map& map = function( args... );
                
                // we have no information about the contents of the sprite::Map
                // at compile-time, so this is essentially a runtime version of
                // the logic further down.
                // this access function will retrieve a value from a sprite::Map
                // given a key, and return it as a string:
                auto access = [&map](const auto& key) -> std::string {
                    if(key.subs.empty())
                        throw InvalidArgumentException("sprite::Map needs sub variable to be accessed like this: ", key.name,".variable");
                    auto ref = map[key.subs.front()];
                    if(not ref.get().valid())
                        throw InvalidArgumentException("Cannot find given parameter ", key,". ", Meta::name<decltype(key)>());
                    
                    // for string-types we can directly return the contents
                    // of the string and there is no need to convert:
                    if (ref.template is_type<std::string>()) {
                        return ref.template value<std::string>();
                    } else if (ref.template is_type<file::Path>()) {
                        return ref.template value<file::Path>().str();
                    }
                    
                    // otherwise we are out of luck and need to use
                    // more complex logic, encapsulated inside sprite::Map's
                    // valueString lambda:
                    return ref.get().valueString();
                };
                
                // "args" would be the key here, so realistically we only
                // have one args. but we don't know that beforehand, so we use
                // the generic version here:
                static_assert(sizeof...(args) == 1, "Expected exactly one argument for sprite::Map access.");
                return ( access(args), ... );
                
            }
            else if constexpr(_clean_same<sprite::ConstReference, return_type> 
                              || _clean_same<sprite::Reference, return_type>)
            {
                // here we simply get a constreference from a sprite::Map, meaning
                // we can directly retrieve its value
                auto r = function(std::forward<decltype(args)>(args)...);
                if(not r.get().valid())
                    return "null";
                
                // we know the type (return_type) here at compile-time,
                // so we can use constexpr logic here (unlike above).
                // for string-types we can directly return the contents
                // of the string and there is no need to convert
                if constexpr(_clean_same<std::string, return_type>) {
                    return r.template value<std::string>();
                } else if constexpr(_clean_same<file::Path, return_type>) {
                    return r.template value<file::Path>().str();
                }
                
                // otherwise we need to use sprite::Maps valueString
                // to convert the value to a string recursively:
                return r.get().valueString();
                
            } else {
                // no special handling for the given type -- use the default
                // Meta::toStr method that can return the string for any value
                return Meta::toStr( function(std::forward<decltype(args)>(args)...) );
            }
        };
    }
    
    /**
      Retrieves the result of the computation or data retrieval.
      @param args Variadic arguments that match the arg_types.
      @return The value of the stored function when applied to the provided arguments.
    */
    template<typename... Args>
    constexpr return_type get(Args... args) const noexcept {
        return function(std::forward<Args>(args)...);
    }
};

// VarBase serves as the base class for Variable.
// It contains utility methods and members that don't depend on the 'return_type'.
// It is designed to be a common interface for all Variable objects regardless of their return type.
template<typename... _Args>
class VarBase {
protected:
    /// The type of the derived class, as a string_view.
    const std::string_view type;
    
public:
    /// Function that converts the stored value to a string, given arguments of type _Args...
    std::function<std::string(_Args...)> value_string;
    
public:
    VarBase(std::string_view type) : type(type) {}
    /// Virtual destructor to enable polymorphic behavior.
    virtual ~VarBase() {}
    
    /**
      Checks if the derived class holds a variable of type T.
      @return true if the derived class holds a variable of type T, false otherwise.
    */
    template<typename T>
    constexpr bool is() const noexcept {
        return type == type_name<T>();
    }
    
    /**
      Safely retrieves the value, given that the caller knows it is of type T.
      @param args Variadic arguments passed to the retrieval function.
      @return The value of the stored function when applied to the provided arguments.
    */
    template<typename T>
    constexpr T value(_Args... args) const noexcept {
        using Target_t = const Variable<T, _Args...>*;
        static_assert(std::same_as<const VarBase<_Args...>*, decltype(this)>);
        
        assert(dynamic_cast<Target_t>(this) != nullptr);
        return static_cast<Target_t>(this)->get(std::forward<_Args>(args)...);
    }
    
    /**
      Returns the type of the derived class.
      @return A string_view representing the type of the derived class.
    */
    constexpr auto class_name() const { return type; }
};

}
}
