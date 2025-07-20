#pragma once
#include <commons.pc.h>

namespace cmn {

// ============================================================================
// ProtectedProperty – thread-safe value container
// ----------------------------------------------------------------------------
// • Trivially-copyable types that are lock-free with std::atomic   ➜ atomic<T>
// • Everything else                                               ➜ T + shared_mutex
//
// Access policy:
//   • get()   → returns a *copy* of the value
//   • set(v)  → replaces the value
//   (No direct reference access is exposed.)
// ============================================================================

template<typename T,
bool UseAtomic =
std::is_trivially_copyable_v<T> &&
std::atomic<T>::is_always_lock_free>
class ProtectedProperty;

// ------------------------------
// Variant 1: Atomic (trivial)
// ------------------------------
template<typename T>
class ProtectedProperty<T, /*UseAtomic=*/true>
{
    std::atomic<T> _value;
    
public:
    constexpr ProtectedProperty() noexcept = default;
    constexpr explicit ProtectedProperty(T initial) noexcept
    : _value(initial) {}
    
    [[nodiscard]] T get() const noexcept {
        return _value.load(std::memory_order_relaxed);
    }
    void set(T v) noexcept {
        _value.store(v, std::memory_order_relaxed);
    }
};

// --------------------------------------
// Variant 2: Mutex-protected (non-trivial)
// --------------------------------------
template<typename T>
class ProtectedProperty<T, /*UseAtomic=*/false>
{
    T _value;
    mutable std::shared_mutex _mtx;
    
public:
    ProtectedProperty() = default;
    explicit ProtectedProperty(const T& initial) : _value(initial) {}
    
    [[nodiscard]] T get() const {
        std::shared_lock lock(_mtx);
        return _value;          // copy
    }
    void set(const T& v) {
        std::unique_lock lock(_mtx);
        _value = v;
    }
};

}
