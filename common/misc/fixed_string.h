#pragma once
#include <commons.pc.h>

namespace cmn::ct::utils {
using Char_t = char;

struct construct_from_pointer_t { };

constexpr auto construct_from_pointer = construct_from_pointer_t{};

template <size_t N> struct fixed_string {
    Char_t content[N] = {};
    size_t real_size{0};
    bool correct_flag{true};
    
    template <typename T> constexpr fixed_string(construct_from_pointer_t, const T * input) noexcept {
        if constexpr (std::is_same_v<T, char>) {
            for (size_t i{0}; i < N; ++i) {
                content[i] = static_cast<uint8_t>(input[i]);
                if ((i == (N-1)) && (input[i] == 0)) break;
                real_size++;
            }
        }
    }
    
    template <typename T> constexpr fixed_string(const std::array<T, N> & in) noexcept: fixed_string{construct_from_pointer, in.data()} { }
    template <typename T> constexpr fixed_string(const T (&input)[N+1]) noexcept: fixed_string{construct_from_pointer, input} { }
    
    constexpr fixed_string(const fixed_string & other) noexcept {
        for (size_t i{0}; i < N; ++i) {
            content[i] = other.content[i];
        }
        real_size = other.real_size;
        correct_flag = other.correct_flag;
    }
    constexpr bool correct() const noexcept {
        return correct_flag;
    }
    constexpr size_t size() const noexcept {
        return real_size;
    }
    constexpr const Char_t * begin() const noexcept {
        return content;
    }
    constexpr const Char_t * end() const noexcept {
        return content + size();
    }
    constexpr Char_t operator[](size_t i) const noexcept {
        return content[i];
    }
    template <size_t M> constexpr bool is_same_as(const fixed_string<M> & rhs) const noexcept {
        if (real_size != rhs.size()) return false;
        for (size_t i{0}; i != real_size; ++i) {
            if (content[i] != rhs[i]) return false;
        }
        return true;
    }
    constexpr operator std::basic_string_view<Char_t>() const noexcept {
        return std::basic_string_view<Char_t>{content, size()};
    }
};

template <> class fixed_string<0> {
    static constexpr Char_t empty[1] = {0};
public:
    template <typename T> constexpr fixed_string(const T *) noexcept {
        
    }
    constexpr fixed_string(std::initializer_list<Char_t>) noexcept {
        
    }
    constexpr fixed_string(const fixed_string &) noexcept {
            
    }
    constexpr bool correct() const noexcept {
        return true;
    }
    constexpr size_t size() const noexcept {
        return 0;
    }
    constexpr const Char_t * begin() const noexcept {
        return empty;
    }
    constexpr const Char_t * end() const noexcept {
        return empty + size();
    }
    constexpr Char_t operator[](size_t) const noexcept {
        return 0;
    }
    constexpr operator std::basic_string_view<Char_t>() const noexcept {
        return std::basic_string_view<Char_t>{empty, 0};
    }
};

template <typename CharT, size_t N> fixed_string(const CharT (&)[N]) -> fixed_string<N-1>;
template <typename CharT, size_t N> fixed_string(const std::array<CharT,N> &) -> fixed_string<N>;

template <size_t N> fixed_string(fixed_string<N>) -> fixed_string<N>;

}
