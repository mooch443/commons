#pragma once

#include <commons.pc.h>
#include <misc/SpriteMap.h>

namespace cmn {

struct Deprecation {
    std::optional<std::string> name;
    std::optional<std::string> replacement;
    std::optional<std::string> note;
    std::function<void(const Deprecation&, std::string_view, sprite::Map&)> apply_fn;
    
    [[nodiscard]] bool valid() const { return name.has_value(); }
    [[nodiscard]] bool has_replacement() const { return replacement.has_value(); }
    [[nodiscard]] bool has_note() const { return note.has_value(); }
    [[nodiscard]] bool has_apply() const { return apply_fn != nullptr; }
};

struct Deprecations {
    struct Entry {
        std::string name;
        std::string replacement;
        std::optional<std::string> note;
        std::function<void(const Deprecation&, std::string_view, sprite::Map&)> apply_fn;
        
        Entry(std::string n, std::string r)
        : name(std::move(n)), replacement(std::move(r)) {}
        
        Entry(std::string n, std::string r, std::function<void(const Deprecation&, std::string_view, sprite::Map&)> fn)
        : name(std::move(n)), replacement(std::move(r)), apply_fn(std::move(fn)) {}
        
        Entry(std::string n, std::string r, std::string nt, std::function<void(const Deprecation&, std::string_view, sprite::Map&)> fn)
        : name(std::move(n)), replacement(std::move(r)), note(std::move(nt)), apply_fn(std::move(fn)) {}
    };
    
    std::unordered_map<std::string, Deprecation, MultiStringHash, MultiStringEqual> deprecations;
    
    Deprecations() = default;
    Deprecations(Deprecations&&) = default;
    Deprecations(const Deprecations&) = default;
    
    Deprecations& operator=(Deprecations&&) = default;
    Deprecations& operator=(const Deprecations&) = default;
    
    Deprecations(std::initializer_list<Entry> entries) {
        for (auto& e : entries) {
            Deprecation dep{
                .name = e.name,
                .replacement = e.replacement,
                .note = e.note,
                .apply_fn = e.apply_fn
            };
            deprecations[e.name] = std::move(dep);
        }
    }
    
    [[nodiscard]] bool is_deprecated(std::string_view) const;
    [[nodiscard]] bool correct_deprecation(std::string_view name, std::string_view val, const sprite::Map* additional, sprite::Map& output) const;
};

}
