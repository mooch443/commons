#pragma once

namespace cmn {

template<class T> struct is_container : public std::false_type {};

template<class T, class Alloc>
struct is_container<std::vector<T, Alloc>> : public std::true_type {};

template<class T, size_t Size>
struct is_container<std::array<T, Size>> : public std::true_type {};

template<class T, size_t Size>
struct is_container<std::span<T, Size>> : public std::true_type {};

template<class T> struct is_set : public std::false_type {};
template<class T, class Alloc>
struct is_set<std::set<T, Alloc>> : public std::true_type {};
template<class T, class Alloc>
struct is_set<std::multiset<T, Alloc>> : public std::true_type {};
template<class T, class Alloc>
struct is_set<std::unordered_set<T, Alloc>> : public std::true_type {};
template<class T>
struct is_set<ska::bytell_hash_set<T>> : public std::true_type {};
template<class T, class Alloc>
struct is_set<robin_hood::unordered_set<T, Alloc>> : public std::true_type {};
template<class T, class Alloc>
struct is_set<robin_hood::unordered_flat_set<T, Alloc>> : public std::true_type {};
template<class T, class Alloc>
struct is_set<robin_hood::unordered_node_set<T, Alloc>> : public std::true_type {};
// template<class T, class Alloc>
//struct is_set<tsl::sparse_set<T, Alloc>> : public std::true_type {};

template<class T> struct is_map : public std::false_type {};
template<class T, class Compare, class Alloc>
struct is_map<std::map<T, Compare, Alloc>> : public std::true_type {};
template<class T, class Compare, class Alloc>
struct is_map<std::unordered_map<T, Compare, Alloc>> : public std::true_type {};
template<class T, class Compare, class Alloc>
struct is_map<ska::bytell_hash_map<T, Compare, Alloc>> : public std::true_type {};
//template<class T, class Compare, class Alloc>
//struct is_map<robin_hood::unordered_node_map<T, Compare, Alloc>> : public std::true_type {};
//template<class T, class Compare, class Alloc>
//struct is_map<robin_hood::unordered_flat_map<T, Compare, Alloc>> : public std::true_type {};
template<bool T, size_t S, typename K, typename V, class... Args>
    requires (not std::is_same<V, void>::value)
struct is_map<robin_hood::detail::Table<T, S, K, V, Args...>> : public std::true_type {};
template<bool T, size_t S, typename K, typename V, class... Args>
    requires (std::is_same<V, void>::value)
struct is_set<robin_hood::detail::Table<T, S, K, V, Args...>> : public std::true_type {};

template<class T> struct is_queue : public std::false_type {};
template<class T, class Container>
struct is_queue<std::queue<T, Container>> : public std::true_type {};
template<class T, class Container>
struct is_queue<std::priority_queue<T, Container>> : public std::true_type {};
template<class T, class Allocator>
struct is_queue<std::deque<T, Allocator>> : public std::true_type {};

template<class T> struct is_deque : public std::false_type {};
template<class T, class Allocator>
struct is_deque<std::deque<T, Allocator>> : public std::true_type {};

template< class T >
struct is_pair : public std::false_type {};

template< class T1 , class T2 >
struct is_pair< std::pair< T1 , T2 > > : public std::true_type {};

template<template<class...>class Template, class T>
struct is_instantiation : std::false_type {};
template<template<class...>class Template, class... Ts>
struct is_instantiation<Template, Template<Ts...>> : std::true_type {};

#pragma region basic_concepts
template<typename T, typename U>
concept _clean_same =
    std::same_as<T, typename std::remove_cv<U>::type>;

template<typename T>
concept unsigned_number = (std::unsigned_integral<T> && !cmn::_clean_same<T, bool> && !cmn::_clean_same<T, unsigned char>);

template<typename T>
concept signed_number = (std::signed_integral<T> && !cmn::_clean_same<T, char>);

template <template<class...>class T, class U>
concept _is_instance = (is_instantiation<T, U>::value);

template <typename T>
concept is_shared_ptr
    = _is_instance<std::shared_ptr, T>;
template <typename T>
concept is_unique_ptr
    = _is_instance<std::unique_ptr, T>;

template<typename T>
concept _has_tostr_method = requires(T t) {
    { t.toStr() } -> std::convertible_to<std::string>;
};
template<typename T>
concept _has_fromstr_method = requires() {
    { T::fromStr(std::string()) }
        -> _clean_same<T>;
};

template<typename T, typename K = typename std::remove_cv<T>::type>
concept _has_class_name = requires() {
    { K::class_name() } -> std::convertible_to<std::string>;
};

template<typename T>
concept _is_smart_pointer =
    (is_shared_ptr<T> || is_unique_ptr<T>);

template<typename T>
concept _is_dumb_pointer =
    (std::is_pointer<T>::value) && (!_is_smart_pointer<T>);

template<typename T, typename K = typename std::remove_cv<T>::type>
concept _is_number =
    (!_clean_same<bool, T>) && (std::floating_point<K> || std::integral<K>); //|| std::convertible_to<T, int>);

template<typename T>
concept is_numeric = (!_clean_same<bool, T>) && (std::floating_point<T> || std::integral<T>);

template<typename T>
concept integral_number = (!_clean_same<bool, T>) && std::integral<T>;

namespace check_abs_detail {
    template<typename T>
    concept has_coordinates = requires(T t) {
        { t.x } -> std::convertible_to<float>;
    };

    template<typename T>
    concept has_get = requires(T t) {
        { t.get() } -> std::convertible_to<float>;
    };
}

#pragma region basic_concepts

//! Pretty useful to find out
template <class F, class... Args>
concept invocable = std::is_invocable<F, Args...>::value;

//! To check whether a callable can be called with the arguments
//! specified in a signature like using sig = R(Args...);
template<class F, class R, class ...Args> struct has_similar_args;
template<class F, class R, class ...Args>
struct has_similar_args<F, R(Args...)> {
    static constexpr bool value = std::is_invocable<F, Args...>::value;
};

template<class F, class sig>
concept similar_args = has_similar_args<F, sig>::value;

namespace detail {

template <typename T>
struct return_type : return_type<decltype(&T::operator())>
{ };

template <typename ClassType, typename ReturnType, typename... Args>
struct return_type<ReturnType(ClassType::*)(Args...) const>
{
    using type = ReturnType;
};

template <typename T>
struct arg_type : arg_type<decltype(&T::operator())>
{ };

template <typename ClassType, typename ReturnType, typename Args>
struct arg_type<ReturnType(ClassType::*)(Args) const>
{
    using type = Args;
};

}

//! Finds the first argument (...T) that matches the single(!) argument
//! type of the given function object, applies it and returns
//! what the given function returns.
template<typename F,
         typename SearchFor = typename detail::arg_type<F>::type,
         typename R = typename detail::return_type<F>::type,
         typename... T>
R find_argtype_apply(F&& fn, T&&... args) {
    // helper function that finds the correct part of the
    // argument list to call fn with, with exactly the
    // parameter type:
    auto f_impl = [&fn](auto& self, auto&& h, auto&&... t) -> R {
        if constexpr ( not std::same_as<std::remove_cvref_t<SearchFor>, std::remove_cvref_t<decltype(h)>> )
        {
            // have not found the correct type yet, search
            // next argument...
            if constexpr(std::same_as<R, void>) {
                self(self,std::forward<decltype(t)>(t)...);
            } else
                return self(self,std::forward<decltype(t)>(t)...);
        } else {
            // function is callable with h, call it and return
            // recursively:
            if constexpr(std::same_as<R, void>) {
                fn(std::forward<decltype(h)>(h));
            } else
                return fn(std::forward<decltype(h)>(h));
        }
    };

    if constexpr(std::same_as<R, void>) {
        f_impl(f_impl, std::forward<T>(args)...);
    } else {
        return f_impl(f_impl, std::forward<T>(args)...);
    }
}

namespace detail {

template <typename F, size_t... Is>
constexpr auto index_apply_impl(F&& f, std::index_sequence<Is...>) {
    return f(std::integral_constant<size_t, Is> {}...);
}

template <size_t N, typename F>
constexpr auto index_apply(F&& f) {
    return index_apply_impl(std::forward<F>(f), std::make_index_sequence<N>{});
}

}

template <class Tuple, class F>
constexpr auto apply_to_tuple(Tuple&& t, F&& f) {
    return detail::index_apply<std::tuple_size<std::remove_cvref_t<Tuple>>{}>(
        [&](auto... Is) { return f(get<Is>(std::forward<Tuple>(t))...); });
}

template<typename T>
concept set_type = is_set<T>::value;

template<typename T>
concept container_type = is_container<T>::value;

template<typename T>
concept map_type = is_map<T>::value;

template<class T> struct is_bytell : public std::false_type {};
template<class T, class Compare, class Alloc>
struct is_bytell<ska::bytell_hash_map<T, Compare, Alloc>> : public std::true_type {};

template<class T> struct is_robin_hood_map : public std::false_type {};
template<bool T, size_t S, typename K, typename V, class... Args>
    requires (not std::is_same<V, void>::value)
struct is_robin_hood_map<robin_hood::detail::Table<T, S, K, V, Args...>> : public std::true_type {};

template<class T> struct is_robin_hood_set : public std::false_type {};
template<bool T, size_t S, typename K, typename V, class... Args>
    requires (std::is_same<V, void>::value)
struct is_robin_hood_set<robin_hood::detail::Table<T, S, K, V, Args...>> : public std::true_type {};

template<typename T>
concept robin_hood_type = is_robin_hood_map<T>::value || is_robin_hood_set<T>::value;


template<typename T>
concept set_or_container = container_type<T> || set_type<T>;

template <typename T>
struct is_tuple final {
    using type = std::false_type;
};
template <typename... Ts>
struct is_tuple<std::tuple<Ts...>> final {
using type = std::true_type;
};

template <typename T>
using is_tuple_t = typename is_tuple<T>::type;

template <typename T>
constexpr auto is_tuple_v = is_tuple_t<T>{};

}
