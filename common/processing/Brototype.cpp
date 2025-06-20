#include "Brototype.h"
#include <processing/DLList.h>

namespace cmn::CPULabeling {

std::unordered_set<Brototype*> Brototype::brototypes() {
    static std::unordered_set<Brototype*> _brototypes;
    return _brototypes;
}

Brototype::Brototype(size_t reserve_hint) {
   // Print("Allocating.");
#if defined(DEBUG_MEM)
    std::lock_guard guard(mutex());
    brototypes().insert(this);
#endif
    _lines.reserve(reserve_hint);
    _pixel_starts.reserve(reserve_hint);
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
    auto&  A  = pixel_starts();
    auto&  AL = lines();
    const auto&  B  = b->pixel_starts();
    const auto&  BL = b->lines();

    if (A.empty()) {
        A .insert(A .end(), B .begin(), B .end());
        AL.insert(AL.end(), BL.begin(), BL.end());
        return;
    }

    A .reserve(A .size() + B .size());
    AL.reserve(AL.size() + BL.size());

    if (AL.back() < BL.front()) {
        A .insert(A .end(), B .begin(), B .end());
        AL.insert(AL.end(), BL.begin(), BL.end());
        return;
    }
    if (BL.back() < AL.front()) {
        A .insert(A .begin(), B .begin(), B .end());
        AL.insert(AL.begin(), BL.begin(), BL.end());
        return;
    }

    auto  it0 = A .begin();
    auto  Lt0 = AL.begin();
    auto  it1 = B .begin();
    auto  Lt1 = BL.begin();
    const auto Bend = B .end();
    const auto BLend = BL.end();     // cached
          auto Aend = A .end();      // updated only after an insert
    auto ALend = AL.end();

    Line_t currA = *Lt0;             // cached dereference
    Line_t currB = *Lt1;
    bool condition = currB < currA;

    for (; it1!=Bend && it0!=Aend;) {
        if (condition) {
            const auto start  = it1;
            const auto Lstart = Lt1;
            std::size_t runLen = 0;

            do {                     // advance B run that precedes currA
                ++Lt1;
                ++it1;
                ++runLen;
                
                if (Lt1 == BLend || it1 == Bend)
                    break;
                currB = *Lt1;
                condition = currB < currA;
            } while (condition);

            it0  = A .insert(it0 , start , it1) + runLen;
            Lt0  = AL.insert(Lt0, Lstart, Lt1) + runLen;
            Aend = A .end();         // **only here** does A change
            ALend = AL.end();
        }
        else {
            ++it0;
            ++Lt0;
            if (Lt0 != ALend)  {   // refresh cached value when we move
                currA = *Lt0;
                condition = currB < currA;
            }
        }
    }
    
    assert(B.end() == Bend);
    if(it1!=Bend) {
        A .insert(A .end(), it1, Bend);
        AL.insert(AL.end(), Lt1, BLend);
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
