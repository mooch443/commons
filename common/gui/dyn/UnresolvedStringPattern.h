#pragma once
#include <commons.pc.h>
#include <gui/dyn/VarProps.h>

namespace cmn::gui::dyn {
struct Context;
struct State;
class CurrentObjectHandler;
}

namespace cmn::pattern {

struct Prepared;
struct Unprepared;
struct PreparedPattern;

//using PreparedPattern = std::variant<std::string_view, Prepared, Prepared*>;
using PreparedPatterns = std::vector<PreparedPattern>;

using UnpreparedPattern = std::variant<std::string_view, Unprepared, std::string>;
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
    static UnresolvedStringPattern prepare_static(std::string_view);
    std::string realize(const gui::dyn::Context& context, gui::dyn::State& state);
    
    std::string toStr() const;
};

struct ResolveContext {
    const gui::dyn::Context& context;
    gui::dyn::State& state;
    const std::shared_ptr<gui::dyn::CurrentObjectHandler>& handler;
    std::string& output;
};

template<bool B> constexpr bool no_context_required() requires (not B) { return false; }

template<bool resolve_directly,
         typename Return_t = std::conditional_t<resolve_directly, void, UnpreparedPatterns>>
Return_t parse_words(
   std::string_view pattern,
   [[maybe_unused]] std::conditional_t<resolve_directly, ResolveContext&&, bool> ctx = no_context_required<resolve_directly>());

struct PreparedPattern {
    union {
        void*empty;
        char ch;
        std::string_view sv;
        std::string* str;
        Prepared* prepared;
        Prepared* ptr;
    } value{nullptr};
    
    enum Type {
        NONE,
        SV,
        PREPARED,
        POINTER,
        STRING,
        CHAR
    } type = NONE;

    PreparedPattern() = default;
    ~PreparedPattern() {
        if(type == STRING)
            delete value.str;
    }
    PreparedPattern(const PreparedPattern& other)
        : value(other.value), type(other.type)
    {
        if(type == STRING)
            value.str = new std::string(*other.value.str);
    }
    PreparedPattern(PreparedPattern&& other) noexcept
        : value(other.value), type(std::exchange(other.type, NONE))
    {}
    PreparedPattern& operator=(const PreparedPattern& other) {
        if(this != &other) {
            PreparedPattern copy(other);
            std::swap(value, copy.value);
            std::swap(type, copy.type);
        }
        return *this;
    }
    PreparedPattern& operator=(PreparedPattern&& other) noexcept {
        if(this != &other) {
            if(type == STRING)
                delete value.str;
            value = other.value;
            type = std::exchange(other.type, NONE);
        }
        return *this;
    }
    
    void resolve(std::string& c, UnresolvedStringPattern& pattern, const std::shared_ptr<gui::dyn::CurrentObjectHandler>&, const gui::dyn::Context& context, gui::dyn::State& state);
    
    bool operator==(const PreparedPattern& other) const noexcept;
    
    bool operator<(const PreparedPattern& other) const noexcept;
    
    std::string toStr() const;
    
    /// Only STRING owns its payload; Prepared nodes belong to all_patterns.
    static PreparedPattern make_char(char ch) {
        PreparedPattern p;
        p.value.ch = ch;
        p.type = CHAR;
        return p;
    }
    static PreparedPattern make_string(std::string str) {
        if(str.size() == 1)
            return make_char(str.front());
        PreparedPattern p;
        p.value.str = new std::string(std::move(str));
        p.type = STRING;
        return p;
    }
    static PreparedPattern make_sv(std::string_view s) {
        if(s.size() == 1)
            return make_char(s.front());
        PreparedPattern p;
        p.value.sv = s;
        p.type = SV;
        return p;
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
    std::vector<std::variant<std::string_view, UnpreparedPatterns>> subs;
    bool optional{false};
    bool html{false};
};

struct Prepared {
    /// Snapshot of variable versions used to validate `_cached_value`.
    struct CachedVariableVersions {
        /// Version of global handler variables.
        uint64_t global{0};
        /// Version of scoped loop variables.
        uint64_t scoped{0};
        bool operator==(const CachedVariableVersions&) const = default;
    };

    std::string_view original;
    std::vector<PreparedPatterns> parameters{};
    std::vector<PreparedPatterns> subs{};
    gui::dyn::VarProps resolved{};
    
    /// Cached resolved output for this prepared node.
    std::optional<std::string> _cached_value{};
    /// Variable-version snapshot captured when `_cached_value` was computed.
    std::optional<CachedVariableVersions> _cached_variable_versions{};
    bool has_children{false};
    
    //Prepared(const Unprepared& unprepared);
    std::string toStr() const;
    static consteval std::string_view class_name() { return "Prepared"; }
    
    void resolve(UnresolvedStringPattern&, std::string&, const std::shared_ptr<gui::dyn::CurrentObjectHandler>&,  const gui::dyn::Context& context, gui::dyn::State& state);
    const std::optional<std::string>& cached() const {
        return _cached_value;
    }
    void reset() {
        _cached_value.reset();
        _cached_variable_versions.reset();
    }
    
    bool operator==(const Prepared&) const = default;
    
    static Prepared* get(Unprepared& unprepared);
};

}
