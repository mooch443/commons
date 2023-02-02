#pragma once

#include <utility>
#include <concepts>
#include <algorithm>

namespace cmn {

namespace package {

/**
 * Here begins the path to encapsulated generic lambdas.
 */

//! This class is dervied from. Required to reduce one template dependency, which
//! we will need to SAVE the generic lambda, but not to execute it. Base can only
//! execute (return type + args known), but doesn't know the generic lambda type.
template<class _Rp, class ..._ArgTypes> struct _F_base;
template<class _Rp, class ..._ArgTypes>
struct _F_base<_Rp(_ArgTypes...)> {
    virtual _Rp operator()(_ArgTypes ...) = 0;
    virtual ~_F_base() {}
};

//! This derived class *does* know the generic lambda type _Fp, and of course
//! also the return + args types.
template<class _Fp, class _Rp, class ..._ArgTypes> struct _F_func;
template<class _Fp, class _Rp, class ..._ArgTypes>
struct _F_func<_Fp, _Rp(_ArgTypes...)>
    : public _F_base<_Rp(_ArgTypes...)>
{
    _Fp func;
    
    _F_func(_Fp&& fp)
        : func( std::move(fp) )
    {}

    //! Define virtual method to execute the saved generic lambda.
    //! With a special case for void, which returns no value.
    virtual _Rp operator()(_ArgTypes ... args) override {
        if constexpr(std::same_as<_Rp, void>)
            func(std::forward<_ArgTypes>(args)...);
        else
            return func(std::forward<_ArgTypes>(args)...);
    }
};

//! This structure is the one that is being called initially. Here, we deduce all types
//! using the constructor:
//!
//! auto number = std::make_unique<int>(5);
//! F package([number = std::move(number)](){
//!     printf("%d\n", *ptr);
//! })
//!
//! The package can then be moved around and executed, without actually knowing
//! the type of the generic lambda, and despite it being move-only.
template<class _Rp, class ..._ArgTypes> struct F;
template<class _Rp, class ..._ArgTypes>
struct F<_Rp(_ArgTypes...)>
{
    using __base_t = _F_base<_Rp(_ArgTypes...)>;
    
    //! this stores the generic lambda
    __base_t *_fn{nullptr};
    
    //! default constructor needed in some storage situations
    F() noexcept {}

    //! The main constructor, which takes a generic lambda and determines the
    //! type of this structure and __base.
    template <typename Ts>
    explicit F(Ts && ts) // move lambda into _fn / encapsulated
        : _fn((__base_t*)new _F_func<Ts, _Rp(_ArgTypes...)>( std::move(ts) ))
    { }

    //! Don't forget to free memory.
    ~F() {
        if(_fn) delete _fn;
    }

    //! Execute the function with parameters.
    //! Simply use the __base_t class' operator().
    template<typename ... _Args>
    _Rp operator()(_Args&& ... args)
    {
        assert(_fn != nullptr);
        return (*_fn)(std::forward<_Args>(args)...);
    }
    
    // copying is evil
    F(const F&) = delete;
    F& operator=(const F&) = delete;

    // moving is OK
    F(F&& f) noexcept {
        std::swap(_fn, f._fn);
    }
    F& operator=(F&& other) {
        F(std::move(other)).swap(*this);
        return *this;
    }
    
    // swapping is cool
    void swap(F& other){
        std::swap(_fn, other._fn);
    }
};

template<typename R, typename... Args>
struct promised {
    using signature = R(Args...);
    package::F<signature> package;
    std::promise<R> promise;

    template<typename K>
    promised(K&& fn) : package(std::move(fn)) { }

    promised& operator=(promised&& other) = default;
    promised& operator=(const promised& other) = delete;

    promised(promised&& other) = default;
    promised(const promised& other) = delete;

    R operator()(Args... args) {
        try {
            if constexpr (!std::same_as<void, R>) {
                promise.set_value(package(std::forward<Args>(args)...));
            }
            else {
                package(std::forward<Args>(args)...);
                promise.set_value();
            }
        }
        catch (...) {
            promise.set_exception(std::current_exception());
        }
    }

    auto get_future() {
        return promise.get_future();
    }
};

}

template<typename R>
inline auto pack(auto&& F) {
    return package::F<R>(std::move(F));
}

}
