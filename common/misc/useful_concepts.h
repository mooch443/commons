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
template<bool T, size_t S, class... Args>
struct is_map<robin_hood::detail::Table<T, S, Args...>> : public std::true_type {};

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

}
