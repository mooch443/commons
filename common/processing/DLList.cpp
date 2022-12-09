#include "DLList.h"
#include <processing/Brototype.h>

namespace cmn::CPULabeling {

Node::Node(std::unique_ptr<Brototype>&& obj, DLList* parent)
    : parent(parent), obj(std::move(obj))
{
#if defined(DEBUG_MEM)
    std::lock_guard guard(mutex());
    created_nodes().insert(this);
#endif
}

void Node::init(DLList* parent) {
    this->parent = parent;
}

void invalidate();

Node::~Node() {
    invalidate();
#if defined(DEBUG_MEM)
    std::lock_guard guard(mutex());
    created_nodes().erase(this);
#endif
}

void Node::invalidate() {
    Brototype::move_to_cache(parent, obj);
    
    auto p = std::move(parent);
    auto pre = std::move(prev);
    auto nex = std::move(next);
    
    prev = nullptr;
    next = nullptr;
    parent = nullptr;
    
    assert(!next);
    assert(!prev);
    assert(!parent);
    
    if(nex) {
        nex->prev = pre;
    } else if(p && p->_end && p->_end == this)
        p->_end = pre;
    
    if(pre) {
        pre->next = nex;
    } else if(p && p->_begin && p->_begin == this)
        p->_begin = nex;
    
    assert(!next);
    assert(!prev);
    //assert(!parent);
}

void Node::move_to_cache(Node::Ref& node) {
    move_to_cache(node.get());
    node = nullptr;
}

void Node::move_to_cache(Node::Ptr node) {
    if(!node)
        return;
    
    if(node->parent)
        node->invalidate();
}

std::unique_ptr<Brototype> DLList::Cache::broto() {
    //std::lock_guard guard(_mutex);
    if(_brotos.empty())
        return nullptr;
    auto ptr = std::move(_brotos.back());
    _brotos.pop_back();
    return ptr;
}

void DLList::Cache::node(typename Node::Ref& ptr) {
    //std::lock_guard guard(_mutex);
    if(_nodes.empty())
        return;
    ptr = std::move(_nodes.back());
    _nodes.pop_back();
}

void DLList::Cache::receive(typename Node::Ref&& ref) {
    //std::lock_guard guard(_mutex);
    assert(!ref || ref->next == nullptr);
    assert(!ref || ref->prev == nullptr);
    if(ref) ref->parent = nullptr;
    _nodes.emplace_back(std::move(ref));
}
void DLList::Cache::receive(std::unique_ptr<Brototype>&& ptr) {
    //std::lock_guard guard(_mutex);
    _brotos.emplace_back(std::move(ptr));
}

std::unique_ptr<DLList> DLList::from_cache() {
    std::lock_guard g(cmutex());
    if(!caches().empty()) {
        auto ptr = std::move(caches().back());
        caches().pop_back();
        return ptr;
    }
    return std::make_unique<DLList>();
}

void DLList::to_cache(std::unique_ptr<DLList>&& ptr) {
    ptr->clear();
    
    std::lock_guard g(cmutex());
    caches().emplace_back(std::move(ptr));
    //ptr = nullptr;
}

Node::Ptr DLList::insert(Node::Ptr ptr) {
    assert(!ptr->prev && !ptr->next);
    
    if(_end) {
        assert(!_end->next);
        assert(_end != ptr);
        ptr->prev = _end;
        _end->next = ptr;
        _end = ptr;
        
        assert(!_end->next);
        
    } else {
        _begin = ptr;
        _end = _begin;
        assert(!ptr->next);
        assert(!ptr->prev);
    }
    
    if(!ptr->parent)
        ptr->init(this);
    
    return ptr;
}

void DLList::insert(Node::Ptr& ptr, std::unique_ptr<Brototype>&& obj) {
    //ptr.release_check();
    Node::Ref ref;
    cache().node(ref);
    
    if(!ref) {
        ref = Node::make(std::move(obj), this);
    } else {
        assert(!ref->next);
        assert(!ref->prev);
        
        ref->init(this);
        ref->obj = std::move(obj);
    }
    
    ptr = ref.get();
    insert(ptr);
    _owned.emplace_back(std::move(ref));
}

DLList::~DLList() {
    clear();
}

void DLList::clear() {
    _source.clear();
    
    auto ptr = std::move(_begin);
    while (ptr != nullptr) {
        auto next = ptr->next;
        auto p = ptr;
        Node::move_to_cache(p);
        ptr = std::move(next);
    }
    _end = nullptr;
}

}
