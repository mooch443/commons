#pragma once
#include <commons.pc.h>
#include <misc/useful_concepts.h>
#include <file/Path.h>
#include <misc/SpriteMap.h>
#include <misc/DisplayValue.h>

namespace cmn::gui::dyn {

/// @brief Base class template for Variable.
//template<bool, bool, typename... _Args>
template<typename... _Args>
class VarBase;

template<typename T>
concept reference = (std::is_lvalue_reference_v<T> &&
                     !std::is_const_v<std::remove_reference_t<T>>);

template<typename T>
concept const_reference = (std::is_lvalue_reference_v<T> &&
                           std::is_const_v<std::remove_reference_t<T>>);

/**
  Represents a typed variable that holds a function.
  The function encapsulates some computation or data retrieval.
*/
template<class return_type, typename... arg_types>
//class Variable : public VarBase<reference<return_type>, const_reference<return_type>, arg_types...> {
class Variable : public VarBase<arg_types...> {
private:
    using Base_t = VarBase<arg_types...>;
    
    /// Human-readable string representing the type of 'return_type'.
    static constexpr const std::type_info* const _type_info = &typeid(return_type);
    static constexpr std::string_view refcode = cmn::type_name<return_type>();
    
protected:
    /// Function that encapsulates the computation or data retrieval.
    std::function<return_type(arg_types...)> function;
    
public:
    using Base_t::value_string;
    using Base_t::value_json;
    
public:
    /**
      Constructor that takes a function object and stores it.
      @param fn A std::function object that matches the return_type and arg_types.
    */
    constexpr Variable(std::function<return_type(arg_types...)> fn) noexcept
        :   Base_t(refcode, _type_info, reference<return_type>, const_reference<return_type>),
            function(std::move(fn))
    {
        assert(static_cast<bool>(function) && "Variable constructed with empty function");
        
        if constexpr(is_container<return_type>::value) {
            this->_is_vector = true;
        }
        
        // Lambda to convert the captured function's return value to a string
        // if we encounter a string type, we can already return without converting
        // or adding any quotes
        value_string = [this]<typename... Args>(Args&&... args) -> std::string {
            if constexpr(_clean_same<sprite::Map&, return_type>) {
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
                        return map.toStr();
                        //throw InvalidArgumentException("sprite::Map needs sub variable to be accessed like this: ", key.name,".variable");
                    auto ref = map[key.subs.front()];
                    if(not ref.get().valid())
                        throw InvalidArgumentException("Cannot find given parameter ", key,". ", Meta::name<decltype(key)>());
                    
                    return sprite::display_property(ref.get());
                };
                
                // "args" would be the key here, so realistically we only
                // have one args. but we don't know that beforehand, so we use
                // the generic version here:
                static_assert(sizeof...(args) == 1, "Expected exactly one argument for sprite::Map access.");
                return ( access(args), ... );
                
            } else {
                // no special handling for the given type -- use the default
                // Meta::toStr method that can return the string for any value
                return sprite::display_object<return_type>( function(std::forward<Args>(args)...) );
            }
        };
        
        if constexpr(has_to_json_method<std::remove_cvref_t<return_type>>)
        {
            value_json = [this]<typename... Args>(Args&&... args) -> glz::json_t
            {
                return function(std::forward<Args>(args)...).to_json();
            };
        }
    }
    
    /**
      Retrieves the result of the computation or data retrieval.
      @param args Variadic arguments that match the arg_types.
      @return The value of the stored function when applied to the provided arguments.
    */
    template<typename... Args>
    constexpr return_type get(Args&&... args) const {
        assert(function != nullptr);
        return function(std::forward<Args>(args)...);
    }
};

// VarBase serves as the base class for Variable.
// It contains utility methods and members that don't depend on the 'return_type'.
// It is designed to be a common interface for all Variable objects regardless of their return type.
template<//bool is_ref, bool is_const_ref,
         typename... _Args>
class VarBase {
protected:
    /// The type of the derived class, as a string_view.
    const std::string_view type;
    const std::type_info* const _type_info;
    const bool is_ref;
    const bool is_const_ref;
    bool _is_vector{false};
    
public:
    /// Function that converts the stored value to a string, given arguments of type _Args...
    std::function<std::string(_Args...)> value_string;
    std::function<glz::json_t(_Args...)> value_json;
    
public:
    VarBase(std::string_view type, const std::type_info* const type_info, bool is_ref, bool is_const_ref)
        : type(type), _type_info(type_info), is_ref(is_ref), is_const_ref(is_const_ref)
    {
        //Print("Creating variable ", demangle(type_info->name()), " == ", type);
    }
    /// Virtual destructor to enable polymorphic behavior.
    virtual ~VarBase() {}
    
    /**
      Checks if the derived class holds a variable of type T.
      @return true if the derived class holds a variable of type T, false otherwise.
    */
    template<typename T>
    constexpr bool is() const noexcept {
        static constexpr const std::type_info* const other = &typeid(T);
        //Print("Creating variable ", demangle(other->name()), " == ", type, " vs ", type_name<T>());
        if constexpr(std::same_as<glz::json_t, T>) {
            return _type_info == other
                    || value_json != nullptr;
        } else
            return _type_info == other;
        //return type == type_name<T>();
    }
    
    template<typename T>
    constexpr bool is_strict() const noexcept {
        static constexpr const std::type_info* const other = &typeid(T);
        //Print("Creating variable ", demangle(other->name()), " == ", type, " vs ", type_name<T>());
        return _type_info == other;
        //return type == type_name<T>();
    }
    
    /**
      Checks if the derived class holds a variable of type T.
      @return true if the derived class holds a variable of type T, false otherwise.
    */
    constexpr bool is_vector() const noexcept {
        return this->_is_vector;
    }
    
    template<typename BaseT>
    constexpr auto access_value(auto&& fn, _Args&&... args) const {
        static_assert(std::same_as<const VarBase<_Args...>*, decltype(this)>);
        
        if constexpr(std::same_as<BaseT, glz::json_t>)
        {
            /// we are trying to access this value as glz::json
            if(not is_strict<glz::json_t>()
               && value_json != nullptr)
            {
                /// but we aren't a *json_t* directly, instead
                /// we have to use the *value_json* method here
                static_assert(not reference<BaseT> && not const_reference<BaseT>);
                return fn(value_json(std::forward<_Args>(args)...));
            }
        }
        
        if(is_const_reference()) {
            return fn(value<const BaseT&>(std::forward<_Args>(args)...));
        } else if(is_reference()) {
            return fn(value<BaseT&>(std::forward<_Args>(args)...));
        } else {
            return fn(value<BaseT>(std::forward<_Args>(args)...));
        }
    }
    
    /**
      Safely retrieves the value, given that the caller knows it is of type T.
      @param args Variadic arguments passed to the retrieval function.
      @return The value of the stored function when applied to the provided arguments.
    */
    template<typename T>
    constexpr T value(_Args&&... args) const {
        //static_assert(std::same_as<const VarBase<is_ref, is_const_ref, _Args...>*, decltype(this)>);
        static_assert(std::same_as<const VarBase<_Args...>*, decltype(this)>);
        
        using Target_t = std::conditional_t<const_reference<T>,
            const Variable<const T&, _Args...>*,
            std::conditional_t<reference<T>,
                const Variable<T&, _Args...>*,
                const Variable<T, _Args...>*>
        >;
        
        assert(dynamic_cast<Target_t>(this) != nullptr);
        //Print("Fetching ", cmn::type_name<Target_t>());
        return static_cast<Target_t>(this)->get(std::forward<_Args>(args)...);
    }
    
    constexpr bool is_reference() const { return is_ref; }
    constexpr bool is_const_reference() const { return is_const_ref; }
    
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
