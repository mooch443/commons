#pragma once
#include <commons.pc.h>
#include <gui/dyn/VarProps.h>

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
    std::unique_ptr<std::string> original;
    PreparedPatterns objects;
    std::optional<size_t> typical_length;
    std::vector<Prepared*> all_patterns;
    
    UnresolvedStringPattern() = default;
    UnresolvedStringPattern(const UnresolvedStringPattern& other);
    UnresolvedStringPattern(UnresolvedStringPattern&& other);
    UnresolvedStringPattern& operator=(UnresolvedStringPattern&& other);
    UnresolvedStringPattern& operator=(const UnresolvedStringPattern& other);
    
    ~UnresolvedStringPattern();
    
    static UnresolvedStringPattern prepare(std::string_view);
    std::string realize(const gui::dyn::Context& context, gui::dyn::State& state);
    
    std::string toStr() const;
};

struct PreparedPattern {
    union {
        std::string_view sv;
        Prepared* prepared;
        Prepared* ptr;
    } value{};
    
    enum Type {
        SV,
        PREPARED,
        POINTER
    } type;
    
    void resolve(std::string& c, UnresolvedStringPattern& pattern, const gui::dyn::Context& context, gui::dyn::State& state);
    
    bool operator==(const PreparedPattern& other) const noexcept;
    
    bool operator<(const PreparedPattern& other) const noexcept;
    
    std::string toStr() const;
    
    /// alternative constructors to maintain aggregate type
    static PreparedPattern make_sv(std::string_view s) {
        return PreparedPattern{s, SV};
    }
    static PreparedPattern make_prepared(Prepared* pRep) {
        PreparedPattern p;
        p.value.prepared = pRep;
        p.type           = PREPARED;
        return p;
    }
    static PreparedPattern make_pointer(Prepared* pRep) {
        PreparedPattern p;
        p.value.ptr = pRep;
        p.type      = POINTER;
        return p;
    }
};

struct Unprepared {
    std::string_view original;
    std::string_view name;
    std::vector<std::variant<std::string_view, UnpreparedPatterns>> parameters;
    std::vector<std::string_view> subs;
    bool optional{false};
    bool html{false};
};

struct Prepared {
    std::string_view original;
    std::vector<PreparedPatterns> parameters;
    gui::dyn::VarProps resolved;
    
    std::optional<std::string> _cached_value;
    bool has_children{false};
    
    //Prepared(const Unprepared& unprepared);
    std::string toStr() const;
    static std::string class_name() { return "Prepared"; }
    
    void resolve(UnresolvedStringPattern&, std::string&, const gui::dyn::Context& context, gui::dyn::State& state);
    const std::optional<std::string>& cached() const {
        return _cached_value;
    }
    void reset() {
        _cached_value.reset();
    }
    
    bool operator==(const Prepared&) const = default;
    
    static Prepared* get(const Unprepared& unprepared);
};

}
