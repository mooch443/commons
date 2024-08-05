#pragma once

namespace cmn {

enum class FormatColorNames {
    BLACK,
    DARK_BLUE,
    DARK_GREEN,
    DARK_CYAN,
    DARK_RED,
    PURPLE,
    DARK_YELLOW,
    GRAY,
    DARK_GRAY,
    BLUE,
    GREEN,
    CYAN,
    RED,
    PINK,
    YELLOW,
    WHITE,
    DARK_PINK,
    LIGHT_CYAN,
    LIGHT_GRAY,
    ORANGE,
    INVALID
};

//static_assert(std::is_trivial_v<FormatColorNames::Class>, "Trivial enums please.");

using FormatColor_t = FormatColorNames;
//namespace FormatColor = FormatColorNames;
using FormatColor = FormatColorNames;

enum class FormatterType {
    UNIX,
    TAGS,
    HTML,
    NONE
};

}
