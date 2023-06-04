#include "SpriteProperty.h"
#include "SpriteMap.h"
//#include "types.h"

namespace cmn {
    namespace sprite {
        //Property<bool> PropertyType::InvalidProp(NULL);
        
        /*template<>
        PropertyType::operator const bool&() const {
            return _valid;
        }*/
        void PropertyType::copy_to(Map *other) const {
            if(not other)
                throw std::invalid_argument("Other map is nullptr.");
            
            if(not valid())
                throw std::invalid_argument("Cannot copy uninitialized propertytype.");
            /*other._set_value_from_string = _set_value_from_string;
            other._type_name = _type_name;
            other._enum_values = _enum_values;
            other._enum_index = _enum_index;
            other._is_array = _is_array;
            other._is_enum = _is_enum;
            other._valid = _valid;*/
        }
    }
}
