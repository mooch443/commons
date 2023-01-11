#pragma once

#include <misc/defines.h>
#include <misc/metastring.h>
#include <misc/checked_casts.h>

namespace cmn {

template<typename Base>
class BFrame_t {
public:
    using number_t = typename Base::value_type;
    //static constexpr number_t invalid = -1;

private:
    Base _frame;
    
public:
    constexpr BFrame_t() = default;
    template<typename T>
        requires std::unsigned_integral<T>
    explicit constexpr BFrame_t(T frame)
        : _frame(static_cast<number_t>(frame))
    { }
    template<typename T>
        requires _clean_same<T, number_t> && std::signed_integral<T>
    explicit constexpr BFrame_t(T frame)
    {
        if (frame >= 0)
            _frame = frame;
        else
            throw std::invalid_argument("Do not initialize with negative numbers.");
    }
    
    template<typename T>
        requires std::signed_integral<T> && (!_clean_same<T, number_t>)
    explicit constexpr BFrame_t(T frame)
    {
        if (frame >= 0)
            _frame = narrow_cast<number_t>(frame);
        else
            throw std::invalid_argument("Do not initialize with negative numbers.");
    }
    
    constexpr void invalidate() noexcept {
        _frame.reset();
    }
    
    constexpr auto get() const {
        return (number_t)_frame.value();
    }
    [[nodiscard]] constexpr bool valid() const noexcept {
        /*if constexpr(std::same_as<Base, int32_t>)
            return _frame != invalid;
        else*/
            return (bool)_frame;
    }

    constexpr auto operator<=>(const BFrame_t& other) const {
        if(not valid() && not other.valid())
            return std::strong_ordering::equal;
       return get() <=> other.get();
    }
    constexpr bool operator==(const BFrame_t& other) const {
        if(valid() != other.valid())
            return false;
        else if(not valid() && not other.valid())
            return true;
       return get() == other.get();
    }
    constexpr inline bool operator!=(const BFrame_t& other) const {
        return not operator==(other);
    }
    
    /*constexpr BFrame_t operator-() const {
        return BFrame_t(-get());
    }*/
    
    constexpr BFrame_t& operator+=(const BFrame_t& other) {
        _frame.value() += other._frame.value();
        return *this;
    }
    constexpr BFrame_t& operator-=(const BFrame_t& other) {
        if(_frame.value() >= other._frame.value())
            _frame.value() -= other._frame.value();
        else
            invalidate();
        return *this;
    }
    
    constexpr BFrame_t& operator--() {
        if(_frame.value() == 0)
            invalidate();
        else
            --_frame.value();
        return *this;
    }
    constexpr BFrame_t& operator++() {
        ++_frame.value();
        return *this;
    }
    
    constexpr BFrame_t operator-(const BFrame_t& other) const {
        if(other.get() > get())
            return BFrame_t();
        return BFrame_t(get() - other.get());
    }
    constexpr BFrame_t operator+(const BFrame_t& other) const {
        return BFrame_t(get() + other.get());
    }
    constexpr BFrame_t operator/(const BFrame_t& other) const {
        return BFrame_t(get() / other.get());
    }
    constexpr BFrame_t operator*(const BFrame_t& other) const {
        return BFrame_t(get() * other.get());
    }
    
    [[nodiscard]] constexpr BFrame_t try_sub(BFrame_t other) const {
        return *this >= other
                    ? *this - other
                    : BFrame_t(0u);
    }

    [[nodiscard]] std::string toStr() const {
        return valid() ? Meta::toStr<number_t>(get()) : "null";
    }
    [[nodiscard]] static std::string class_name() {
        return "frame";
    }
    [[nodiscard]] static BFrame_t fromStr(const std::string& str) {
        return (str == "null" || str == "-1")
                    ? BFrame_t()
                    : BFrame_t(cmn::Meta::fromStr<number_t>(str));
    }
};

using Frame_t = BFrame_t<std::optional<int32_t>>;

constexpr Frame_t operator""_f(const unsigned long long int value) {
    // intentional static cast, but probably unsafe.
    return Frame_t(value);
}

constexpr inline Frame_t min(Frame_t A, Frame_t B) {
    return std::min(A, B);
}

constexpr inline Frame_t max(Frame_t A, Frame_t B) {
    return std::max(A, B);
}

constexpr inline Frame_t saturate(Frame_t value, Frame_t from, Frame_t to) {
    return std::clamp(value, from, to);
}

constexpr inline Frame_t abs(Frame_t A) noexcept {
    return A;
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
