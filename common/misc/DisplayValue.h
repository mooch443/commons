#pragma once

#include <misc/SpriteMap.h>

namespace cmn::sprite {

template<typename T>
inline std::string display_object(T&& object, std::string_view null_optional = "null") {
    if constexpr(_is_instance<std::optional, std::remove_cvref_t<T>>) {
        if(!object.has_value())
             return std::string(null_optional);
        return display_object(object.value(), null_optional);

    } else if constexpr(_clean_same<std::string, T>) {
        return object;

    } else if constexpr(_clean_same<file::Path, T>) {
        return object.str();

    } else if constexpr(_clean_same<sprite::ConstReference, T> 
                        || _clean_same<sprite::Reference, T>)
    {
        if(not object.valid())
            return std::string(null_optional);

        if(object.is_optional()) {
            if (not object.optional_has_value()) {
                return std::string(null_optional);
            }

            auto dereferenced = object.dereference_optional();
            if (not dereferenced) {
                return std::string(null_optional);
            }

            return display_object(*dereferenced, null_optional);
            
        } else if(object.get().template is_type<file::Path>()) {
            return object.get().template value<file::Path>().str();
        } else if(object.get().template is_type<std::string>()) {
            return object.get().template value<std::string>();
        }
        return object.get().valueString();

    }

    return Meta::toStr(std::forward<T>(object));
}

inline std::string display_property(const PropertyType& property,
                                    std::string_view null_optional = "null") {
    if (property.is_optional()) {
        if (!property.optional_has_value()) {
            return std::string(null_optional);
        }

        auto dereferenced = property.dereference_optional();
        if (!dereferenced) {
            return std::string(null_optional);
        }

        return display_property(*dereferenced, null_optional);
    }

    if (property.is_type<std::string>()) {
        return property.value<std::string>();
    }

    if (property.is_type<file::Path>()) {
        return property.value<file::Path>().str();
    }

    return property.valueString();
}

inline std::string display_property(const Reference& reference,
                                    std::string_view null_optional = "null") {
    return reference.valid()
           ? display_property(reference.get(), null_optional)
           : std::string(null_optional);
}

inline std::string display_property(const ConstReference& reference,
                                    std::string_view null_optional = "null") {
    return reference.valid()
           ? display_property(reference.get(), null_optional)
           : std::string(null_optional);
}

} // namespace cmn::sprite
