#pragma once

namespace cmn {


template<typename T, typename K>
constexpr T clamp(T value, K min, K max) {
    return (value < min) ? min : (value > max) ? max : value;
}

template<typename T, std::size_t N>
class MathArray : public std::array<T, N> {
public:
    using std::array<T, N>::array; // Inherit constructors
    
    constexpr MathArray(std::initializer_list<T> init) {
        std::copy(init.begin(), init.end(), this->begin());
    }
    
    // Addition operator with range checks
    constexpr MathArray operator+(const MathArray& other) const {
        MathArray result;
        for (std::size_t i = 0; i < N; ++i) {
            result[i] = clamp((*this)[i] + other[i], std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        }
        return result;
    }
    
    // Subtraction operator with range checks
    constexpr MathArray operator-(const MathArray& other) const {
        MathArray result;
        for (std::size_t i = 0; i < N; ++i) {
            if constexpr (std::is_unsigned<T>::value) {
                result[i] = (*this)[i] > other[i] ? (*this)[i] - other[i] : 0;
            } else {
                result[i] = clamp((*this)[i] - other[i], std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
            }
        }
        return result;
    }
    
    // Multiplication operator with range checks
    constexpr MathArray operator*(const MathArray& other) const {
        MathArray result;
        for (std::size_t i = 0; i < N; ++i) {
            result[i] = clamp((*this)[i] * other[i], std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        }
        return result;
    }
    
    // Division operator with range checks
    constexpr MathArray operator/(const MathArray& other) const {
        MathArray result;
        for (std::size_t i = 0; i < N; ++i) {
            if (other[i] == 0) {
                throw std::invalid_argument("Division by zero");
            }
            result[i] = clamp((*this)[i] / other[i], std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        }
        return result;
    }
    
    // Addition assignment operator with range checks
    constexpr MathArray& operator+=(const MathArray& other) {
        for (std::size_t i = 0; i < N; ++i) {
            (*this)[i] = clamp((*this)[i] + other[i], std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        }
        return *this;
    }
    
    // Subtraction assignment operator with range checks
    constexpr MathArray& operator-=(const MathArray& other) {
        for (std::size_t i = 0; i < N; ++i) {
            if constexpr (std::is_unsigned<T>::value) {
                (*this)[i] = (*this)[i] > other[i] ? (*this)[i] - other[i] : 0;
            } else {
                (*this)[i] = clamp((*this)[i] - other[i], std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
            }
        }
        return *this;
    }
    
    // Multiplication assignment operator with range checks
    constexpr MathArray& operator*=(const MathArray& other) {
        for (std::size_t i = 0; i < N; ++i) {
            (*this)[i] = clamp((*this)[i] * other[i], std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        }
        return *this;
    }
    
    // Division assignment operator with range checks
    constexpr MathArray& operator/=(const MathArray& other) {
        for (std::size_t i = 0; i < N; ++i) {
            if (other[i] == 0) {
                throw std::invalid_argument("Division by zero");
            }
            (*this)[i] = clamp((*this)[i] / other[i], std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        }
        return *this;
    }
    
    // Print the array
    friend std::ostream& operator<<(std::ostream& os, const MathArray& arr) {
        os << "[ ";
        for (const auto& elem : arr) {
            os << static_cast<int>(elem) << " ";
        }
        os << "]";
        return os;
    }
};

using RGBArray = MathArray<uchar, 3>;

template<class T>
struct is_rgb_array : public std::false_type {};

template<class T, size_t Size>
struct is_rgb_array<MathArray<T, Size>> : public std::true_type {};

template <typename Func, typename T, std::size_t N, template<typename, std::size_t> class Array, std::size_t... Indices>
constexpr auto apply_function_impl(const Array<T, N>& arr, Func func, std::index_sequence<Indices...>) {
    return Array<T, N>{static_cast<T>(func(std::get<Indices>(arr)))...};
}

template <typename Func, typename T, std::size_t N, template<typename, std::size_t> class Array>
constexpr auto apply_function(const Array<T, N>& arr, Func func) {
    return apply_function_impl(arr, func, std::make_index_sequence<N>{});
}

template<typename K, typename T = K::value_type>
    requires (is_rgb_array<K>::value)
constexpr inline auto saturate(K val, T min = 0, T max = 255) {
    return apply_function(val, [&](auto x) { return std::clamp(T(x), min, max); });
}

}
