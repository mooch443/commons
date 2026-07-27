#pragma once
#include <commons.pc.h>

namespace cmn::gui {
    namespace pointer {
        class Events {
            uint8_t _value;

        public:
            static const Events None;
            static const Events Hover;
            static const Events Click;
            static const Events Drag;
            static const Events Scroll;
            static const Events All;

            constexpr Events() : _value(0) {}
            explicit constexpr Events(uint8_t value) : _value(value) {}

            std::string toStr() const {
                if(*this == None) return "none";
                if(*this == Hover) return "hover";
                if(*this == Click) return "click";
                if(*this == Drag) return "drag";
                if(*this == Scroll) return "scroll";
                if(*this == All) return "all";

                std::string value = "[";
                uint8_t debug = 0;
                for(auto event : {Hover, Click, Drag, Scroll}) {
                    if(bool(event & *this)) {
                        if(debug != 0)
                            value += ",";
                        value += event.toStr();
                        debug |= event._value;
                    }
                }
                return value + "]";
            }
            static Events fromStr(cmn::StringLike auto&& value) {
                const std::string text(value);
                if(not text.empty() && text.front() == '[') {
                    Events result;
                    for(auto event : Meta::fromStr<std::vector<Events>>(text))
                        result |= event;
                    return result;
                }

                const auto token = utils::lowercase(Meta::fromStr<std::string>(text));
                if(token == "none") return None;
                if(token == "hover") return Hover;
                if(token == "click") return Click;
                if(token == "drag") return Drag;
                if(token == "scroll") return Scroll;
                if(token == "all") return All;
                throw InvalidArgumentException("Unknown pointer event '", token, "'.");
            }

            friend constexpr bool operator==(Events, Events) = default;

            friend constexpr Events operator|(Events lhs, Events rhs) {
                lhs._value |= rhs._value;
                return lhs;
            }

            friend constexpr Events operator&(Events lhs, Events rhs) {
                lhs._value &= rhs._value;
                return lhs;
            }

            constexpr Events& operator|=(Events rhs) {
                _value |= rhs._value;
                return *this;
            }

            [[nodiscard]] constexpr explicit operator bool() const {
                return _value != 0;
            }

            static consteval std::string_view class_name() { return "Events"; }
        };

        inline constexpr Events Events::None{0};
        inline constexpr Events Events::Hover{1 << 0};
        inline constexpr Events Events::Click{1 << 1};
        inline constexpr Events Events::Drag{1 << 2};
        inline constexpr Events Events::Scroll{1 << 3};
        inline constexpr Events Events::All{(1 << 0) | (1 << 1) | (1 << 2) | (1 << 3)};
    }

    namespace Keyboard {
        enum Codes
        {
            Unknown = -1, ///< Unhandled key
            A = 0,        ///< The A key
            B,            ///< The B key
            C,            ///< The C key
            D,            ///< The D key
            E,            ///< The E key
            F,            ///< The F key
            G,            ///< The G key
            H,            ///< The H key
            I,            ///< The I key
            J,            ///< The J key
            K,            ///< The K key
            L,            ///< The L key
            M,            ///< The M key
            N,            ///< The N key
            O,            ///< The O key
            P,            ///< The P key
            Q,            ///< The Q key
            R,            ///< The R key
            S,            ///< The S key
            T,            ///< The T key
            U,            ///< The U key
            V,            ///< The V key
            W,            ///< The W key
            X,            ///< The X key
            Y,            ///< The Y key
            Z,            ///< The Z key
            Num0,         ///< The 0 key
            Num1,         ///< The 1 key
            Num2,         ///< The 2 key
            Num3,         ///< The 3 key
            Num4,         ///< The 4 key
            Num5,         ///< The 5 key
            Num6,         ///< The 6 key
            Num7,         ///< The 7 key
            Num8,         ///< The 8 key
            Num9,         ///< The 9 key
            Escape,       ///< The Escape key
            LControl,     ///< The left Control key
            LShift,       ///< The left Shift key
            LAlt,         ///< The left Alt key
            LSystem,      ///< The left OS specific key: window (Windows and Linux), apple (MacOS X), ...
            RControl,     ///< The right Control key
            RShift,       ///< The right Shift key
            RAlt,         ///< The right Alt key
            RSystem,      ///< The right OS specific key: window (Windows and Linux), apple (MacOS X), ...
            Menu,         ///< The Menu key
            LBracket,     ///< The [ key
            RBracket,     ///< The ] key
            SemiColon,    ///< The ; key
            Comma,        ///< The , key
            Period,       ///< The . key
            Quote,        ///< The ' key
            Slash,        ///< The / key
            BackSlash,    ///< The \ key
            Tilde,        ///< The ~ key
            Equal,        ///< The = key
            Dash,         ///< The - key
            Space,        ///< The Space key
            Return,       ///< The Return key
            BackSpace,    ///< The Backspace key
            Tab,          ///< The Tabulation key
            PageUp,       ///< The Page up key
            PageDown,     ///< The Page down key
            End,          ///< The End key
            Home,         ///< The Home key
            Insert,       ///< The Insert key
            Delete,       ///< The Delete key
            Add,          ///< The + key
            Subtract,     ///< The - key
            Multiply,     ///< The * key
            Divide,       ///< The / key
            Left,         ///< Left arrow
            Right,        ///< Right arrow
            Up,           ///< Up arrow
            Down,         ///< Down arrow
            Numpad0,      ///< The numpad 0 key
            Numpad1,      ///< The numpad 1 key
            Numpad2,      ///< The numpad 2 key
            Numpad3,      ///< The numpad 3 key
            Numpad4,      ///< The numpad 4 key
            Numpad5,      ///< The numpad 5 key
            Numpad6,      ///< The numpad 6 key
            Numpad7,      ///< The numpad 7 key
            Numpad8,      ///< The numpad 8 key
            Numpad9,      ///< The numpad 9 key
            F1,           ///< The F1 key
            F2,           ///< The F2 key
            F3,           ///< The F3 key
            F4,           ///< The F4 key
            F5,           ///< The F5 key
            F6,           ///< The F6 key
            F7,           ///< The F7 key
            F8,           ///< The F8 key
            F9,           ///< The F9 key
            F10,          ///< The F10 key
            F11,          ///< The F11 key
            F12,          ///< The F12 key
            F13,          ///< The F13 key
            F14,          ///< The F14 key
            F15,          ///< The F15 key
            Pause,        ///< The Pause key
            
            KeyCount      ///< Keep last -- the total number of keyboard keys
        };
    }
    
    using namespace Keyboard;
    
    constexpr static const Codes code_map[128] = {
        Codes::Unknown,
        Codes::Unknown,  Codes::Unknown,  Codes::Unknown, Codes::Unknown,  Codes::Unknown,
        Codes::Unknown,  Codes::Unknown,  Codes::BackSpace,Codes::Tab,     Codes::Unknown,
        Codes::Unknown,  Codes::Unknown,  Codes::Return,  Codes::Unknown,  Codes::Unknown,
        Codes::Unknown,  Codes::Unknown,  Codes::Unknown, Codes::Unknown,  Codes::Unknown,
        Codes::Unknown,  Codes::Unknown,  Codes::Unknown, Codes::Unknown,  Codes::Unknown,
        Codes::Unknown,  Codes::Escape,   Codes::Unknown, Codes::Unknown,  Codes::Unknown,
        Codes::Unknown,  Codes::Space,    Codes::Unknown, Codes::Quote,    Codes::Unknown,
        Codes::Unknown,  Codes::Unknown,  Codes::Unknown, Codes::Unknown,  Codes::LBracket,
        Codes::RBracket, Codes::Multiply, Codes::Add,     Codes::Comma,    Codes::Subtract,
        Codes::Period,   Codes::Slash,    Codes::Num0,    Codes::Num1,     Codes::Num2,
        Codes::Num3,     Codes::Num4,     Codes::Num5,    Codes::Num6,     Codes::Num7,
        Codes::Num8,     Codes::Num9,     Codes::Unknown, Codes::SemiColon,Codes::Unknown,
        Codes::Equal,    Codes::Unknown,  Codes::Unknown, Codes::Unknown,  Codes::A,
        Codes::B,        Codes::C,        Codes::D,       Codes::E,        Codes::F,
        Codes::G,        Codes::H,        Codes::I,       Codes::J,        Codes::K,
        Codes::L,        Codes::M,        Codes::N,       Codes::O,        Codes::P,
        Codes::Q,        Codes::R,        Codes::S,       Codes::T,        Codes::U,
        Codes::V,        Codes::W,        Codes::X,       Codes::Y,        Codes::Z,
        Codes::Unknown,  Codes::BackSlash,Codes::Unknown, Codes::Unknown,  Codes::Unknown,
        Codes::Unknown,  Codes::A,        Codes::B,       Codes::C,        Codes::D,
        Codes::E,        Codes::F,        Codes::G,       Codes::H,        Codes::I,
        Codes::J,        Codes::K,        Codes::L,       Codes::M,        Codes::N,
        Codes::O,        Codes::P,        Codes::Q,       Codes::R,        Codes::S,
        Codes::T,        Codes::U,        Codes::V,       Codes::W,        Codes::X,
        Codes::Y,        Codes::Z,        Codes::Unknown, Codes::Unknown,  Codes::Unknown,
        Codes::Unknown,  Codes::Delete
    };
    
    enum EventType {
        HOVER,
        MBUTTON,
        SELECT,
        KEY,
        TEXT_ENTERED,
        MMOVE,
        WINDOW_RESIZED,
        SCROLL,
        DRAG
    };
    
    struct WindowResizedEvent {
        Float2_t width, height;
    };
    
    struct MouseMoveEvent {
        Float2_t x, y;
    };
    
    struct MouseButtonEvent {
        int button;
        bool pressed;
        bool started_here;
        Float2_t x, y;
    };
    
    struct SelectEvent {
        bool selected;
    };
    
    struct HoverEvent {
        bool hovered;
        Float2_t x, y;
    };
    
    struct KeyEvent {
        Codes code;
        bool pressed;
        bool shift;
    };
    
    struct TextEvent {
        char c;
    };
    
    struct ScrollEvent {
        Float2_t dx,dy;
    };
    
    struct DragEvent {
        Float2_t x,y;
        Float2_t rx, ry;
    };
    
    class Event {
    public:
        EventType type;
        
        union {
            MouseMoveEvent move;
            MouseButtonEvent mbutton;
            KeyEvent key;
            SelectEvent select;
            HoverEvent hover;
            TextEvent text;
            WindowResizedEvent size;
            ScrollEvent scroll;
            DragEvent drag;
        };
        
        Event(EventType t) : type(t) {}
    };
}
