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

template<typename T>
concept UseAtomic =
std::is_trivially_copyable_v<T> &&
std::atomic<T>::is_always_lock_free;

template<typename T, bool UseAtomic>
class _ProtectedProperty;

// ------------------------------
// Variant 1: Atomic (trivial)
// ------------------------------
template<typename T>
class _ProtectedProperty<T, true>
{
    std::atomic<T> _value;
    
public:
    constexpr _ProtectedProperty() noexcept = default;
    constexpr _ProtectedProperty(T initial) noexcept : _value(initial) {}
    
    _ProtectedProperty(const _ProtectedProperty&) = delete;
    _ProtectedProperty& operator=(const _ProtectedProperty&) = delete;
    _ProtectedProperty(_ProtectedProperty&&) = default;
    _ProtectedProperty& operator=(_ProtectedProperty&&) = default;
    
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
class _ProtectedProperty<T, false>
{
    T _value;
    mutable std::shared_mutex _mtx;
    
public:
    _ProtectedProperty() = default;
    _ProtectedProperty(const T& initial) : _value(initial) {}
    _ProtectedProperty(T&& initial) : _value(std::move(initial)) {}
    
    [[nodiscard]] T get() const {
        std::shared_lock lock(_mtx);
        return _value;          // copy
    }
    void set(const T& v) {
        std::unique_lock lock(_mtx);
        _value = v;
    }
    void set(T&& v) {
        std::unique_lock lock(_mtx);
        _value = std::move(v);
    }
};

template<typename T>
using ProtectedProperty = _ProtectedProperty<T, UseAtomic<T>>;

}
