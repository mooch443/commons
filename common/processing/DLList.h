#pragma once

#include <commons.pc.h>
#include <processing/Node.h>
#include <processing/Brototype.h>
#include <processing/Source.h>

namespace cmn::CPULabeling {


class DLList {
    template<typename ValueType, typename NodeType>
    class _iterator
    {
    public:
        typedef _iterator self_type;
        typedef ValueType value_type;
        typedef ValueType& reference;
        typedef ValueType* pointer;
        typedef std::forward_iterator_tag iterator_category;
        typedef int difference_type;
        constexpr _iterator(pointer ptr) : ptr_(ptr), next(ptr_ && ptr_->next ? ptr_->next : nullptr) { }
        
        constexpr self_type operator++() {
            ptr_ = next;
            next = ptr_ && ptr_->next ? ptr_->next : nullptr;
            return ptr_;
        }
        
        constexpr pointer& operator*() { return ptr_; }
        constexpr pointer& operator->() { return ptr_; }
        constexpr bool operator==(const self_type& rhs) const { return ptr_ == rhs.ptr_; }
        constexpr bool operator!=(const self_type& rhs) const { return ptr_ != rhs.ptr_; }
        
    public:
        pointer ptr_;
        pointer next;
    };
    
public:
    typedef _iterator<Node, Node::Ptr> iterator;
    typedef _iterator<const Node, const Node::Ptr> const_iterator;
    
    Node::Ptr _begin = nullptr;
    Node::Ptr _end = nullptr;
    std::vector<Node::Ref> _owned;
    
    struct Cache {
        //std::mutex _mutex;
        std::vector<typename Node::Ref> _nodes;
        std::vector<std::unique_ptr<Brototype>> _brotos;
        
        std::unique_ptr<Brototype> broto();
        
        void node(typename Node::Ref& ptr);
        
        void receive(typename Node::Ref&& ref);
        void receive(std::unique_ptr<Brototype>&& ptr);
    };
    
    GETTER_NCONST(Cache, cache)
    GETTER_NCONST(Source, source)
    
public:
    static auto& cmutex() {
        static std::mutex _mutex;
        return _mutex;
    }
    
    static auto& caches() {
        static std::vector<std::unique_ptr<DLList>> _caches;
        return _caches;
    }
    
    inline static std::unique_ptr<DLList> from_cache();
    inline static void to_cache(std::unique_ptr<DLList>&& ptr);
    
    Node::Ptr insert(Node::Ptr ptr);
    
public:
    void insert(Node::Ptr& ptr, std::unique_ptr<Brototype>&& obj);
    
    ~DLList();
    void clear();
    
    constexpr iterator begin() { return _iterator<Node, Node::Ptr>(_begin); }
    constexpr iterator end() { return _iterator<Node, Node::Ptr>(nullptr); }
};

}
