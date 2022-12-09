#pragma once

#include <commons.pc.h>

namespace cmn::CPULabeling {

class Brototype;
class DLList;

class Node {
public:
    DLList *parent = nullptr;
    
/*private:
    size_t _retains = 0;
    
public:
    void retain() {
        ++_retains;
    }
    
    constexpr bool unique() const {
        return _retains == 0;
    }
    
    void release() {
        assert(_retains > 0);
        --_retains;
    }
    
    struct Ref {
        Node::Ptr obj = nullptr;
        
        Ref(const Ref& other) : obj(other.obj) {
            if(obj)
                obj->retain();
        }
        
        Ref(Node::Ptr obj) : obj(obj) {
            if(obj)
                obj->retain();
        }
        
        constexpr Ref(Ref&& other) noexcept {
            obj = other.obj;
            other.obj = nullptr;
        }

        Ref() = default;
        
        ~Ref() {
            release_check();
        }
        
        Ref& operator=(const Ref& other) {
            if(obj != other.obj) {
                release_check();
                obj = other.obj;
                if(obj)
                    obj->retain();
            }
            return *this;
        }
        
        Ref& operator=(Ref&& other) noexcept {
            if(obj != other.obj) {
                release_check();
                obj = other.obj;
                
            } else if(obj) {
                obj->release();
            }
            
            other.obj = nullptr;
            return *this;
        }
        
        constexpr bool operator==(Node::Ptr other) const { return other == obj; }
        constexpr bool operator==(const Ref& other) const { return other.obj == obj; }
        constexpr bool operator!=(Node::Ptr other) const { return other != obj; }
        constexpr bool operator!=(const Ref& other) const { return other.obj != obj; }
        
        constexpr Node& operator*() const { assert(obj != nullptr); return *obj; }
        constexpr Node::Ptr operator->() const { assert(obj != nullptr); return obj; }
        constexpr Node::Ptr get() const { return obj; }
        constexpr operator bool() const { return obj != nullptr; }
        
        void release_check();
    };*/
    
public:
    using Ref = std::unique_ptr<Node>;
    using Ptr = Node*;
    Node::Ptr prev = nullptr;
    Node::Ptr next = nullptr;
    std::unique_ptr<Brototype> obj;
    
    template<class... Args>
    static Ref make(Args... args) {
        return std::make_unique<Node>(std::forward<Args>(args)...);
    }
    
    static auto& mutex() {
        static std::mutex _mutex;
        return _mutex;
    }
    
#if defined(DEBUG_MEM)
    static auto& created_nodes() {
        static std::unordered_set<void*> _created_nodes;
        return _created_nodes;
    }
#endif
    static std::vector<Node::Ref>& pool() {
        static std::vector<Node::Ref> p;
        return p;
    }
    
    static void move_to_cache(Node::Ref& node);
    static void move_to_cache(Node::Ptr node);
    
    Node(std::unique_ptr<Brototype>&& obj, DLList* parent);
    void init(DLList* parent);
    void invalidate();
    
    ~Node();
};

using List_t = DLList;
using Node_t = Node;

}
