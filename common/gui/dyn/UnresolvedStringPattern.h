#pragma once
#include <commons.pc.h>

namespace cmn::gui::dyn {
struct Context;
struct State;
}

namespace cmn::pattern {

struct Prepared;
struct Unprepared;
struct PreparedPattern;

//using PreparedPattern = std::variant<std::string_view, Prepared, Prepared*>;
using PreparedPatterns = std::vector<PreparedPattern>;

using UnpreparedPattern = std::variant<std::string_view, Unprepared>;
using UnpreparedPatterns = std::vector<UnpreparedPattern>;

struct UnresolvedStringPattern {
    PreparedPatterns objects;
    std::optional<size_t> typical_length;
    std::vector<Prepared*> all_patterns;
    
    UnresolvedStringPattern() = default;
    UnresolvedStringPattern(UnresolvedStringPattern&& other);
    UnresolvedStringPattern& operator=(UnresolvedStringPattern&& other);
    
    ~UnresolvedStringPattern();
    
    static UnresolvedStringPattern prepare(std::string_view);
    std::string realize(const gui::dyn::Context& context, gui::dyn::State& state);
};

struct PreparedPattern {
    union {
        std::string_view sv;
        Prepared* prepared;
        Prepared* ptr;
    } value;
    
    enum Type {
        SV,
        PREPARED,
        POINTER
    } type;
    
    void resolve(std::string& c, UnresolvedStringPattern& pattern, const gui::dyn::Context& context, gui::dyn::State& state);
    
    bool operator==(const PreparedPattern& other) const noexcept;
    
    bool operator<(const PreparedPattern& other) const noexcept;
    
    std::string toStr() const;
};


}
