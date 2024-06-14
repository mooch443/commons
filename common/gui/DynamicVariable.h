#pragma once
#include <commons.pc.h>
#include <misc/useful_concepts.h>
#include <file/Path.h>
#include <misc/SpriteMap.h>

namespace cmn::gui::dyn {

/// @brief Base class template for Variable.
template<typename... _Args>
class VarBase;

/**
  Represents a typed variable that holds a function.
  The function encapsulates some computation or data retrieval.
*/
template<class return_type, typename... arg_types>
class Variable : public VarBase<arg_types...> {
private:
    /// Human-readable string representing the type of 'return_type'.
    static constexpr auto refcode = cmn::type_name<return_type>();
    
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
        
        if constexpr(is_container<return_type>::value) {
            this->_is_vector = true;
        }
        
        // Lambda to convert the captured function's return value to a string
        value_string = [this]<typename... Args>(Args&&... args) -> std::string {
            if constexpr(_clean_same<std::string, return_type>) {
                // if we encounter a string type, we can already return without converting
                // or adding any quotes
                return function( std::forward<Args>(args)... );
                
            } else if constexpr(_clean_same<file::Path, return_type>) {
                return function( std::forward<Args>(args)... ).str();
                
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
                auto r = function(std::forward<Args>(args)...);
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
                return Meta::toStr( function(std::forward<Args>(args)...) );
            }
        };
    }
    
    /**
      Retrieves the result of the computation or data retrieval.
      @param args Variadic arguments that match the arg_types.
      @return The value of the stored function when applied to the provided arguments.
    */
    template<typename... Args>
    constexpr return_type get(Args&&... args) const {
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
    bool _is_vector{false};
    
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
      Checks if the derived class holds a variable of type T.
      @return true if the derived class holds a variable of type T, false otherwise.
    */
    constexpr bool is_vector() const noexcept {
        return this->_is_vector;
    }
    
    /**
      Safely retrieves the value, given that the caller knows it is of type T.
      @param args Variadic arguments passed to the retrieval function.
      @return The value of the stored function when applied to the provided arguments.
    */
    template<typename T>
    constexpr T value(_Args&&... args) const {
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

/// @brief Deduction guide for Variable for exactly two arguments.
/// This guide is enabled only when the callable object F has exactly two arguments.
/// It deduces 'return_type' and the two argument types.
template<class F, typename R = typename cmn::detail::return_type<F>::type, typename arg = typename  cmn::detail::arg_types<F>::type, std::size_t Size = std::tuple_size_v<arg>, typename SecondArg = std::tuple_element_t<cmn::detail::static_min<Size - 1u, 1u>(), arg>>
    requires (Size == 2u)
Variable(F&&) -> Variable<R, std::tuple_element_t<0, arg>, SecondArg>;

/// @brief Deduction guide for Variable for exactly one argument.
/// This guide is enabled only when the callable object F has exactly one argument.
/// It deduces 'return_type' and the single argument type.
template<class F, typename R = typename cmn::detail::return_type<F>::type, typename arg = typename  cmn::detail::arg_types<F>::type>
    requires (std::tuple_size_v<arg> == 1u)
Variable(F&&) -> Variable<R, std::tuple_element_t<0, arg>>;

/// @brief Deduction guide for Variable for member function pointers.
/// This guide is for callable objects that are member function pointers.
/// It deduces 'return_type' and the argument types directly from the pointer.
template<typename ReturnType, typename ClassType, typename... Args>
Variable(ReturnType(ClassType::*)(Args...) const) -> Variable<ReturnType, Args...>;

}
