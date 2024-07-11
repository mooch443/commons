#include "Brototype.h"
#include <processing/DLList.h>

namespace cmn::CPULabeling {

std::unordered_set<Brototype*> Brototype::brototypes() {
    static std::unordered_set<Brototype*> _brototypes;
    return _brototypes;
}

Brototype::Brototype() {
   // Print("Allocating.");
#if defined(DEBUG_MEM)
    std::lock_guard guard(mutex());
    brototypes().insert(this);
#endif
}

Brototype::Brototype(const Line_t& line, const uchar* px)
    : _pixel_starts({px}), _lines({line})
{
    //Print("Allocating.");
#if defined(DEBUG_MEM)
    std::lock_guard guard(mutex());
    brototypes().insert(this);
#endif
}

Brototype::~Brototype() {
    //Print("Deallocating.");
#if defined(DEBUG_MEM)
    std::lock_guard guard(mutex());
    brototypes().erase(this);
#endif
}

std::mutex& Brototype::mutex() { static std::mutex m; return m; }

void Brototype::merge_with(const std::unique_ptr<Brototype>& b) {
    auto&        A = pixel_starts();
    auto&       AL = lines();
    
    const auto&  B = b->pixel_starts();
    const auto& BL = b->lines();
    
    if(A.empty()) {
        A .insert(A .end(), B .begin(), B .end());
        AL.insert(AL.end(), BL.begin(), BL.end());
        return;
    }
    
    A .reserve(A .size()+B .size());
    AL.reserve(AL.size()+BL.size());
    
    // special cases
    if(AL.back() < BL.front()) {
        A .insert(A .end(), B .begin(), B .end());
        AL.insert(AL.end(), BL.begin(), BL.end());
        return;
    }
    
    auto it0=A .begin();
    auto Lt0=AL.begin();
    auto it1=B .begin();
    auto Lt1=BL.begin();
    
    for (; it1!=B.end() && it0!=A.end();) {
        if((*Lt1) < (*Lt0)) {
            const auto start = it1;
            const auto Lstart = Lt1;
            do {
                ++Lt1;
                ++it1;
            }
            while (Lt1 != BL.end() && it1 != B.end()
                   && (*Lt1) < (*Lt0));
            it0 = A .insert(it0, start , it1) + (it1 - start);
            Lt0 = AL.insert(Lt0, Lstart, Lt1) + (Lt1 - Lstart);
            
        } else {
            ++it0;
            //++Nt0;
            ++Lt0;
        }
    }
    
    if(it1!=B.end()) {
        A.insert(A.end(), it1, B.end());
        AL.insert(AL.end(), Lt1, BL.end());
    }
}


void Brototype::move_to_cache(List_t* list, typename std::unique_ptr<Brototype>& node) {
    if(!node) {
        return;
    }
    
    //Print("Had ", node->lines().size(), " upon clear.");

    node->lines().clear();
    node->pixel_starts().clear();
    
    if(list)
        list->cache().receive(std::move(node));
    else
        FormatWarning("No list");
    node = nullptr;
}


}
