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

void Brototype::finalize() {
    if(_parent)
        return;
    if(_children.empty())
        return;
    if(_lines.empty())
        return;
    
    size_t N = _lines.size();
    
    for(auto ptr : _children) {
        N += ptr->_lines.size();
    }
    
    _lines.reserve(N);
    _pixel_starts.reserve(N);
    
    //size_t i = 0;
    for(auto ptr : _children) {
        assert(ptr != this);
        if(not ptr->_parent)
            continue;
        if(ptr->lines().empty())
            continue;
        
        assert(ptr->_children.empty());
        //if(int k = ++i % 3;
        //   k == 0)
            merge_with(*ptr);
        //else if(k == 1)
        //    __merge_with(*ptr);
        //else
        //    _merge_with(*ptr);
        
        /// we are done with this
        ptr->lines().clear();
        ptr->pixel_starts().clear();
        ptr->_parent = nullptr;
    }
    
    _children.clear();
}

void Brototype::set_parent(Brototype * bro) {
    if(bro->_parent)
        bro = bro->_parent;
    if(bro == this)
        return;
    
    assert(not bro->_parent);
    assert(bro != this);
    assert( _parent == nullptr || _children.empty() );
    
    /// we are transferring our stuff into our new bro
    if(_parent) {
        if(bro == _parent)
            return;
        
        /// we already have a parent
        //FormatWarning("We are already merged with ", hex(_parent), " indirecting to ", hex(bro), " ", hex(this));
        assert(not _parent->_children.empty());
        assert(_children.empty());
        
        for(auto c : _parent->_children) {
            if(c == this)
                continue;
            assert(c != bro);
            
            assert(c->_parent == _parent);
            assert(c->_children.empty());
            
            c->_parent = bro;
            bro->_children.push_back(c);
        }
        
        _parent->_children.clear();
        bro->_children.push_back(_parent);
        _parent->_parent = bro;
    }
    
    if(not _children.empty()) {
        for(auto c : _children) {
            assert(c != this);
            assert(c->_children.empty());
            assert(c->_parent == this);
            assert(c != bro);
            
            bro->_children.push_back(c);
            c->_parent = bro;
        }
        _children.clear();
    }
    
    bro->_children.push_back(this);
    assert(_children.empty());
    _parent = bro;
}

void Brototype::merge_with(const Brototype& b) {
    // Merge using scratch buffers and swap to avoid repeated shifting
    static thread_local std::vector<const uchar*, NoInitializeAllocator<Pixel>> scratchA;
    static thread_local std::vector<Line_t, NoInitializeAllocator<Line_t>>       scratchL;
    
    auto&  Ap = pixel_starts();
    auto&  Al = lines();
    auto&  Bp = b.pixel_starts();
    auto&  Bl = b.lines();

    size_t  nA = Al.size(), nB = Bl.size();
    size_t  outSize = nA + nB;

    // Ensure our scratch buffers are big enough (reuse static or member buffers!)
    scratchA.resize(outSize);
    scratchL.resize(outSize);

    const uchar* const* aPx = Ap.data();
    const uchar* const* bPx = Bp.data();
    const Line_t*      aLn = Al.data();
    const Line_t*      bLn = Bl.data();

    uchar const** outPx = scratchA.data();
    Line_t*        outLn = scratchL.data();

    size_t i = 0, j = 0, k = 0;
    while (i < nA && j < nB) {
        // find how many B’s come before aLn[i]
        if (bLn[j] < aLn[i]) {
            size_t start = j;
            while (j < nB && bLn[j] < aLn[i]) ++j;
            size_t len = j - start;
            // copy B[start..j) in one go
            std::memcpy(outPx + k, bPx + start, len * sizeof(*bPx));
            std::memcpy(outLn + k, bLn + start, len * sizeof(*bLn));
            k += len;
        } else {
            // copy this single A element
            outPx[k] = aPx[i];
            outLn[k] = aLn[i];
            ++i; ++k;
        }
    }
    // copy any remaining tail
    if (i < nA) {
        size_t rem = nA - i;
        std::memcpy(outPx + k, aPx + i, rem * sizeof(*aPx));
        std::memcpy(outLn + k, aLn + i, rem * sizeof(*aLn));
        k += rem;
    }
    if (j < nB) {
        size_t rem = nB - j;
        std::memcpy(outPx + k, bPx + j, rem * sizeof(*bPx));
        std::memcpy(outLn + k, bLn + j, rem * sizeof(*bLn));
        k += rem;
    }

    // swap in the scratch
    Ap.assign(scratchA.begin(), scratchA.begin() + k);
    Al.assign(scratchL.begin(), scratchL.begin() + k);
}

void Brototype::_merge_with(const Brototype& b)
{
    auto&        A  = pixel_starts();
    auto&        AL = lines();
    const auto&  B  = b.pixel_starts();
    const auto&  BL = b.lines();

    assert(this != &b);
    assert(&A != &B);

    /*------------------------------------------------------------*\
     *           unchanged early-exit / fast-path cases            *
    \*------------------------------------------------------------*/
    if (A.empty()) {
        A .insert(A .end(), B .begin(), B .end());
        AL.insert(AL.end(), BL.begin(), BL.end());
        return;
    }

    A .reserve(A .size() + B .size());
    AL.reserve(AL.size() + BL.size());

    if (AL.back() <  BL.front()) {
        A .insert(A .end(),   B .begin(),  B .end());
        AL.insert(AL.end(),   BL.begin(),  BL.end());
        return;
    }
    if (BL.back() <  AL.front()) {
        A .insert(A .begin(), B .begin(),  B .end());
        AL.insert(AL.begin(), BL.begin(),  BL.end());
        return;
    }

    /*------------------------------------------------------------*\
     *          main merge — iterator → index conversion          *
    \*------------------------------------------------------------*/
    std::size_t idxA   = 0;              // current position in A/AL
    std::size_t idxB   = 0;              // current position in B/BL
    std::size_t sizeA  = A.size();       // live size of A  (grows!)
    const std::size_t sizeB = B.size();  // fixed size of B

    Line_t currA = AL[idxA];
    Line_t currB = BL[idxB];
    bool   condition = currB < currA;    // same flag as before

    while (idxB < sizeB && idxA < sizeA) {
        if (condition) {
            /*------------------------------------------------------*
             * gather a consecutive run of B that precedes currA    *
            *------------------------------------------------------*/
            const std::size_t runStart = idxB;
            std::size_t       runLen   = 0;

            do {
                ++idxB;
                ++runLen;
                if (idxB == sizeB)           break;
                currB     = BL[idxB];
                condition = currB < currA;
            } while (condition);

            /* insert the whole run just before A[idxA] (unchanged) */
            A .insert (A .begin()  + idxA,
                       B .begin()  + runStart,
                       B .begin()  + idxB);
            AL.insert(AL.begin()  + idxA,
                       BL.begin() + runStart,
                       BL.begin() + idxB);

            sizeA += runLen;   // A grew by runLen
            idxA  += runLen;   // skip over the newly inserted block
        }
        else {
            ++idxA;            // advance in A
            if (idxA != sizeA) {
                currA     = AL[idxA];
                condition = currB < currA;
            }
        }
    }

    /*------------------------------------------------------------*\
     *                copy any remaining tail of B                *
    \*------------------------------------------------------------*/
    if (idxB != sizeB) {
        A .insert(A .end(),  B .begin()  + idxB, B .end());
        AL.insert(AL.end(),  BL.begin() + idxB, BL.end());
    }
}

void Brototype::__merge_with(const Brototype& b) {
    auto&  A  = pixel_starts();
    auto&  AL = lines();
    const auto&  B  = b.pixel_starts();
    const auto&  BL = b.lines();
    
    assert(this != &b);
    assert(&A != &B);

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
    bool it1c = it1 != Bend;
    bool it0c = it0 != Aend;

    for (; it1c && it0c; ) {
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

            it0  = A .insert(it0, start , it1) + runLen;
            Lt0  = AL.insert(Lt0, Lstart, Lt1) + runLen;
            Aend = A .end();         // **only here** does A change
            ALend = AL.end();
            
            it1c = it1 != Bend;
            it0c = it0 != Aend;
        }
        else {
            ++it0;
            ++Lt0;
            if (Lt0 != ALend)  {   // refresh cached value when we move
                currA = *Lt0;
                condition = currB < currA;
            }
            
            it0c = it0 != Aend;
        }
    }
    
    assert(B.end() == Bend);
    if(it1!=Bend) {
        A .insert(A .end(), it1, Bend);
        AL.insert(AL.end(), Lt1, BLend);
    }
}

/*void Brototype::merge_with(const Brototype& b) {
    auto&  A  = pixel_starts();
    auto&  AL = lines();
    const auto&  B  = b.pixel_starts();
    const auto&  BL = b.lines();
    
    assert(this != &b);
    assert(&A != &B);

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

    // Fallback for small B: insert each run with binary search
    static constexpr size_t SMALL_MERGE_THRESHOLD = 32;
    //if (B.size() < SMALL_MERGE_THRESHOLD)
    {
        for (size_t i = 0, n = B.size(); i < n; ++i) {
            auto const& line = BL[i];
            auto pos = std::upper_bound(AL.begin(), AL.end(), line);
            auto idx = pos - AL.begin();
            AL.insert(AL.begin() + idx, line);
            A.insert(A.begin() + idx, B[i]);
        }
        return;
    }

    // Merge using scratch buffers and swap to avoid repeated shifting
    static thread_local std::vector<const uchar*> scratchA;
    static thread_local std::vector<Line_t>       scratchAL;
    
    // clear but keep capacity
    scratchA.clear();
    scratchAL.clear();
    
    // only grow if needed
    size_t neededA  = A.size()  + B.size();
    if (scratchA.capacity()  < neededA)  scratchA.reserve(neededA);
    size_t neededAL = AL.size() + BL.size();
    if (scratchAL.capacity() < neededAL) scratchAL.reserve(neededAL);

    std::merge( A.begin(),  A.end(),  B.begin(),  B.end(),  std::back_inserter(scratchA) );
    std::merge( AL.begin(), AL.end(), BL.begin(), BL.end(), std::back_inserter(scratchAL) );

    A.swap(scratchA);
    AL.swap(scratchAL);
    return;
}*/


void Brototype::move_to_cache(List_t* list, typename std::unique_ptr<Brototype>& node) {
    if(!node) {
        return;
    }
    
    //Print("Had ", node->lines().size(), " upon clear.");

    node->lines().clear();
    node->pixel_starts().clear();
    assert( (node->_parent == nullptr) || node->_children.empty() );
    node->_parent = nullptr;
    node->_children.clear();
    
    if(list)
        list->cache().receive(std::move(node));
    else
        FormatWarning("No list");
    node = nullptr;
}


}
