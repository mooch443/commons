#pragma once

#include <misc/defines.h>
#include <misc/metastring.h>
#include <misc/checked_casts.h>

namespace cmn {

class Frame_t {
public:
    using number_t = int32_t;
    static constexpr number_t invalid = -1;

private:
    number_t _frame = invalid;
    
public:
    Frame_t() = default;
    template<typename T>
        requires _clean_same<T, number_t>
    explicit constexpr Frame_t(T frame)
        : _frame(frame)
    { }
    
    template<typename T>
        requires std::is_convertible_v<T, number_t> && (!_clean_same<T, number_t>)
    explicit constexpr Frame_t(T frame)
        : _frame(narrow_cast<number_t>(frame))
    { }
    
    constexpr void invalidate() { _frame = invalid; }
    
    constexpr number_t get() const { return _frame; }
    constexpr bool valid() const { return _frame >= 0; }
    
    constexpr auto operator<=>(const Frame_t& other) const = default;
    
    constexpr Frame_t operator-() const {
        return Frame_t(-get());
    }
    
    constexpr Frame_t& operator+=(const Frame_t& other) {
        _frame += other._frame;
        return *this;
    }
    constexpr Frame_t& operator-=(const Frame_t& other) {
        _frame -= other._frame;
        return *this;
    }
    
    constexpr Frame_t& operator--() {
        --_frame;
        return *this;
    }
    constexpr Frame_t& operator++() {
        ++_frame;
        return *this;
    }
    
    constexpr Frame_t operator-(const Frame_t& other) const {
        return Frame_t(get() - other.get());
    }
    constexpr Frame_t operator+(const Frame_t& other) const {
        return Frame_t(get() + other.get());
    }
    constexpr Frame_t operator/(const Frame_t& other) const {
        return Frame_t(get() / other.get());
    }
    constexpr Frame_t operator*(const Frame_t& other) const {
        return Frame_t(get() * other.get());
    }

    std::string toStr() const {
        return Meta::toStr<number_t>(_frame);
    }
    static std::string class_name() {
        return "frame";
    }
    static Frame_t fromStr(const std::string& str) {
        return Frame_t(cmn::Meta::fromStr<number_t>(str));
    }
};

constexpr Frame_t operator""_f(const unsigned long long int value) {
    // intentional static cast, but probably unsafe.
    return Frame_t(static_cast<Frame_t::number_t>(value));
}

constexpr inline Frame_t min(Frame_t A, Frame_t B) {
    return std::min(A, B);
}

constexpr inline Frame_t max(Frame_t A, Frame_t B) {
    return std::max(A, B);
}

}

namespace std {

template<>
struct hash<cmn::Frame_t>
{
    size_t operator()(const cmn::Frame_t& k) const
    {
        return std::hash<cmn::Frame_t::number_t>{}(k.get());
    }
};

}
