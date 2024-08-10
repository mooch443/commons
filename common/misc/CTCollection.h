#pragma once
#include <commons.pc.h>
#include <misc/fixed_string.h>

namespace cmn::ct {

template<typename... Args>
struct VariedPairCollection {
    static constexpr std::size_t N = sizeof...(Args) / 2;
    static constexpr auto sequence = std::make_index_sequence<N>{};
    
private:
    static constexpr decltype(auto) helper(std::string_view A, auto B) {
        return std::make_tuple(std::make_tuple(A, B));
    }

    static constexpr decltype(auto) helper(std::string_view A, auto B, auto... rest) {
        return std::tuple_cat(std::make_tuple(std::make_tuple(A, B)), helper(rest...));
    }

    struct ApplyToTupleHelper {
        using type = decltype(helper(std::declval<Args>()...));
    };

public:
    using TupleType = typename ApplyToTupleHelper::type;
    TupleType storage;

    constexpr VariedPairCollection(Args... args)
        : storage(helper(args...))
    { }

    constexpr VariedPairCollection& apply(auto&& F) {
        auto fn = [&]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            (F(std::get<0>(std::get<Is>(storage)), std::get<1>(std::get<Is>(storage))), ...);
        };
        fn(sequence);
        return *this;
    }
    
    /**
     * This is the worst method in here, abusing almost all of template
     * programming to achieve something that has long been thought impossible
     * (at least in C++).
     * It is supposed to apply a given lambda to the value at the given key.
     * This only works if the type is the type stored in the map originally
     * without the call-site knowing the type of the value that had been stored.
     *
     * Okay, to be honest of course it doesn't do anything impossible. It only
     * works if the Lambda is not variadic / only the correct types are stored.
     * @param key the key :)
     * @param F the function to be applied taking one argument
     */
    constexpr auto apply(std::string_view key, auto&& F) {
        /// if we keep this mutable then we can actually modify the object
        /// inside the storage from the given function :)
        auto apply_single = [&]<std::size_t i>() mutable -> bool {
            /// can we invoke the given function with the given type?
            /// in this case it *does* compile, even though we never
            /// call the method in some cases. otherwise we would get
            /// a compile error - this is a trade-off:
            if constexpr(std::is_invocable_v<decltype(F),
                         std::tuple_element_t<1, std::tuple_element_t<i, TupleType>>>)
            {
                F(std::get<1>(std::get<i>(storage)));
                return true;
            }
            return false;
        };
        /// outer-most helper function that indexes and returns state
        auto fn = [&]<std::size_t... Is>(std::index_sequence<Is...>) mutable -> bool {
            return ((std::get<0>(std::get<Is>(storage)) == key
                     ? (apply_single.template operator()<Is>()) : false) || ...);
        };
        if (!fn(sequence)) {
            throw std::runtime_error("Key not found, or type mismatch.");
        }
    }
    
    template<typename T, std::size_t i>
    constexpr std::optional<T> retrieve() const {
        if constexpr(std::same_as<std::tuple_element_t<1, std::tuple_element_t<i, TupleType>>, T>)
        {
            return std::get<1>(std::get<i>(storage));
            
        } else if constexpr(std::is_constructible_v<std::tuple_element_t<1, std::tuple_element_t<i, TupleType>>, T>)
        {
            return T{std::get<1>(std::get<i>(storage))};
            
        } else {
            return std::nullopt;
        }
    }
    
    /**
     * Abomination of value-retrieval in a generic heterogenous map
     * that throws an `std::bad_optional_access` exception if the
     * value was not found (for some reason).
     * @param key the key of the value in the dictionary
     */
    template<typename T>
    constexpr T value(std::string_view key) const {
        /// helper lambda that is required to match the
        /// integer index sequence and compile-time iterate
        /// through all elements
        auto fn = [&]<std::size_t... Is>(std::index_sequence<Is...>)
            -> std::optional<T>
        {
            /// assign to an optional if we find the key, then return.
            /// -- abuse the fold expressions and actually lazy-eval(!!)
            /// the tuples values:
            if(std::optional<T> result;
               ( (std::get<0>(std::get<Is>(storage)) == key
                      ? (result = retrieve<T, Is>()).has_value()
                      : false) || ... ))
            {
                return result;
            }
            
            return std::nullopt;
        };

        /// intentional choice to throw if we didnt find the
        /// key we wanted, or it was the wrong type.
        return fn(sequence).value();
    }
};

template<utils::fixed_string... strings>
struct Key {
    static constexpr auto names = std::make_tuple(strings...);
    using TupleType = decltype(names);
    static constexpr std::size_t Size = std::tuple_size_v<TupleType>;
};

template<typename... Args>
struct VarPart {
    using ValueType = std::tuple<Args...>;
    static constexpr std::size_t Size = std::tuple_size_v<ValueType>;
    ValueType values;
    
    constexpr VarPart(Args... args)
    : values{std::make_tuple(args...)}
    {}
};

template<typename Keys, typename... Args>
struct CTCollection {
    using KeysType = Keys;
    using ValueType = std::tuple<Args...>;
    static constexpr std::size_t Size = std::tuple_size_v<ValueType>;
    
    ValueType values;
    
    constexpr CTCollection(Keys, Args... values)
        : values(std::make_tuple(values...))
    {
        static_assert(Keys::Size == Size, "Keys and values must match in length.");
    }
    
    template<utils::fixed_string key>
    constexpr auto get() const {
        constexpr std::size_t index = []<std::size_t... Is>(std::index_sequence<Is...>){
            return (((std::string_view(key) == std::string_view(std::get<Is>(Keys::names))) ? Is : 0) + ...);
        }(std::make_index_sequence<Size>{});
        return std::get<index>(values);
    }

    template<typename F>
    constexpr const CTCollection& apply(F&& fn) const {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            (fn(std::string_view(std::get<Is>(KeysType::names)), std::get<Is>(values)), ...);
        }(std::make_index_sequence<Size>{});
        return *this;
    }

    template<typename F>
    constexpr CTCollection& apply(F&& fn) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            (fn(std::get<Is>(KeysType::names), std::get<Is>(values)), ...);
        }(std::make_index_sequence<Size>{});
        return *this;
    }
};

}
