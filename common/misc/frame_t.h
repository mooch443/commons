#pragma once

#include <commons.pc.h>
#include <misc/checked_casts.h>

namespace cmn {

template<typename Numeric, Numeric InvalidValue = static_cast<Numeric>(-1)>
class TrivialOptional {
    static_assert(std::is_arithmetic<Numeric>::value, "TrivialOptional can only be used with arithmetic types.");

    Numeric value_{InvalidValue};

public:
    using value_type = Numeric;
    
    constexpr TrivialOptional() noexcept = default;
    constexpr TrivialOptional(const TrivialOptional&) noexcept = default;
    constexpr TrivialOptional(TrivialOptional&&) noexcept = default;
    constexpr explicit TrivialOptional(Numeric value) noexcept : value_(value) {}

    constexpr TrivialOptional& operator=(const TrivialOptional&) noexcept = default;
    constexpr TrivialOptional& operator=(TrivialOptional&&) noexcept = default;
    constexpr TrivialOptional& operator=(Numeric value) noexcept {
        value_ = value;
        return *this;
    }

    constexpr bool has_value() const noexcept {
        return value_ != InvalidValue;
    }

    constexpr const Numeric& value() const noexcept {
        assert(has_value());
        return value_;
    }

    constexpr Numeric& value() noexcept {
        assert(has_value());
        return value_;
    }

    constexpr Numeric value_or(Numeric default_value) const noexcept {
        return has_value() ? value_ : default_value;
    }

    constexpr void reset() noexcept {
        value_ = InvalidValue;
    }

    constexpr Numeric& operator*() noexcept {
        return value_;
    }

    constexpr const Numeric& operator*() const noexcept {
        return value_;
    }

    constexpr TrivialOptional& operator+=(Numeric value) noexcept {
        value_ += value;
        return *this;
    }
    constexpr TrivialOptional& operator+=(const TrivialOptional& value) noexcept {
        assert(value.has_value());
        assert(has_value());
        value_ += value;
        return *this;
    }

    constexpr Numeric* operator->() noexcept {
        return &value_;
    }

    constexpr const Numeric* operator->() const noexcept {
        return &value_;
    }

    constexpr explicit operator bool() const noexcept {
        return has_value();
    }

    // Comparison operators
    constexpr friend bool operator==(const TrivialOptional& lhs, const TrivialOptional& rhs) {
        return lhs.value_ == rhs.value_;
    }

    constexpr friend bool operator!=(const TrivialOptional& lhs, const TrivialOptional& rhs) {
        return !(lhs == rhs);
    }
};

template<typename Base>
class BFrame_t {
public:
    using number_t = typename Base::value_type;
    
    // Compile-time check to ensure that number_t is integral
    static_assert(std::is_integral_v<number_t>, "number_t must be integral");
    
private:
    Base _frame;
    
    template<typename T>
    constexpr void checkNonNegative(T frame) const noexcept(not is_debug_mode()) {
        if constexpr (is_debug_mode()) {
            if (frame < 0) {
                throw std::invalid_argument("Do not initialize with negative numbers.");
            }
        }
    }
    
public:
    constexpr BFrame_t() = default;
    template<typename T>
        requires std::unsigned_integral<T>
    explicit constexpr BFrame_t(T frame) noexcept
        : _frame(static_cast<number_t>(frame))
    { }
    
    template<typename T>
        requires _clean_same<T, number_t> && std::signed_integral<T>
    explicit constexpr BFrame_t(T frame) noexcept(not is_debug_mode())
    {
        checkNonNegative(frame);
        _frame = frame;
    }
    
    template<typename T>
        requires std::signed_integral<T> && (!_clean_same<T, number_t>)
    explicit constexpr BFrame_t(T frame)
    {
        checkNonNegative(frame);
        _frame = narrow_cast<number_t>(frame);
    }
    
    constexpr void invalidate() noexcept {
        _frame.reset();
    }
    
    constexpr auto get() const {
        return static_cast<number_t>(_frame.value());
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
    
    constexpr BFrame_t& operator+=(const BFrame_t& other) {
        if(not valid())
            _frame = other._frame;
        else
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
    constexpr BFrame_t& operator++() & {
        ++_frame.value();
        return *this;
    }
    constexpr BFrame_t operator++(int) & {
        BFrame_t temp = *this;
        ++*this;
        return temp;
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
    [[nodiscard]] nlohmann::json to_json() const {
        if(valid())
            return nlohmann::json(get());
        return nullptr;
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

#ifndef NDEBUG
using Frame_t = BFrame_t<std::optional<uint32_t>>;
#else
using Frame_t = BFrame_t<TrivialOptional<uint32_t>>;
#endif

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
