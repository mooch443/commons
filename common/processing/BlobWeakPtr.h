#pragma once

#include <commons.pc.h>

namespace pv {

class Blob;

struct BlobWeakPtr {
    pv::Blob* ptr{};

    constexpr BlobWeakPtr() = default;
    constexpr BlobWeakPtr(pv::Blob* p) : ptr(p) {}
    constexpr operator pv::Blob*() const {
        return ptr;
    }
    constexpr pv::Blob& operator*() { return *ptr; }
    constexpr const pv::Blob& operator*() const { return *ptr; }
    constexpr pv::Blob* operator->() { return ptr; }
    constexpr const pv::Blob* operator->() const { return ptr; }
    std::string toStr() const;
    static std::string class_name() { return "BlobWeakPtr"; }
};

}
