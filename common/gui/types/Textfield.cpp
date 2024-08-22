#include "Textfield.h"
#include <gui/DrawSFBase.h>
#include <misc/Timer.h>
#include <misc/GlobalSettings.h>
#include <gui/types/StaticText.h>
#include <gui/Passthrough.h>

#if __has_include ( <GLFW/glfw3.h> )
    #include <GLFW/glfw3.h>
    #define CMN_CLIPBOARD_GLFW
#elif __has_include ( <clip.h> )
    #include <clip.h>
    #define CMN_CLIPBOARD_CLIP
#endif

namespace cmn::gui {
    constexpr static const Float2_t margin = 5;

    void set_clipboard(const std::string& text) {
#ifdef CMN_CLIPBOARD_GLFW
        glfwSetClipboardString(nullptr, text.c_str());
#elif CMN_CLIPBOARD_CLIP
        clip::set_text(text);
#else
        FormatExcept("Cannot copy text ",text,". Please enable CLIP or GLFW in CMake.");
#endif
    }
    
    std::string get_clipboard() {
#ifdef CMN_CLIPBOARD_GLFW
        auto ptr = glfwGetClipboardString(nullptr);
        if(ptr) {
            return std::string(ptr);
        }
        return std::string();
#elif CMN_CLIPBOARD_CLIP
        std::string paste;
        if(clip::get_text(paste)) {
            return paste;
        }
        
        return std::string();
#else
        FormatExcept("Cannot paste from clipboard. Please enable CLIP or GLFW in CMake.");
        return std::string();
#endif
    }

    std::string::size_type find_first_of_reverse(
                                                     std::string const& str,
                                                     std::string::size_type const pos,
                                                     std::string const& chars)
    {
        if(pos < 1)
            return std::string::npos;
        
        assert(pos >= 1);
        assert(pos <= str.size());
        
        std::string::size_type const res = str.find_last_not_of(chars, pos - 1) + 1;
        return res == pos ? find_first_of_reverse(str, pos - 1, chars)
            : res ? res
            : std::string::npos;
    }
    
    std::tuple<bool, bool> Textfield::system_alt() const {
        bool system = false, alt = false;
        const bool nowindow = GlobalSettings::has("nowindow") && SETTING(nowindow);
        
        // TODO: Must also accept this from Httpd!
        if(!nowindow) {
#if __APPLE__
            system = stage() && (stage()->is_key_pressed(LSystem) || stage()->is_key_pressed(RSystem));
#else
            system = stage() && (stage()->is_key_pressed(LControl) || stage()->is_key_pressed(LControl));
#endif
            alt = stage() && (stage()->is_key_pressed(RAlt) || stage()->is_key_pressed(LAlt));
        }
        
        return {system, alt};
    }

void Textfield::init() {
    if(not _settings.on_tab) {
        _settings.on_tab = OnTab_t{[this](){
            if(parent() && parent()->stage()) {
                std::vector<Textfield*> textfields;
                std::deque<SectionInterface*> q{
                    (SectionInterface*)&parent()->stage()->root()
                };
                
                std::optional<size_t> index;
                while(not q.empty()) {
                    auto root = q.front();
                    q.pop_front();
                    
                    //Print("* searching ", (uint64_t)root, " name=", root->name());
                    
                    apply_to_objects(root->children(), [&](auto c){
                        if(c->type() == Type::ENTANGLED
                           && dynamic_cast<Textfield*>(c))
                        {
                            if(c == this) {
                                index = textfields.size();
                            }
                            textfields.push_back(static_cast<Textfield*>(c));
                            
                        } else if(dynamic_cast<SectionInterface*>(c)) {
                            q.push_front(static_cast<SectionInterface*>(c));
                        }
                    });
                }
                
                //Print("* searching for ", (uint64_t)this);
                std::vector<uint64_t> ptrs;
                for(auto t : textfields) {
                    ptrs.push_back((uint64_t)t);
                }
                //Print("  all textfields: ", ptrs);
                
                if(index.has_value()) {
                    if(textfields.size() >= index.value() && index.value() > 0) {
                        stage()->select(textfields.at(index.value()-1));
                    } else if(not textfields.empty()) {
                        stage()->select(textfields.back());
                    }
                }
                
            }
        }};
    }
    
    _selection_rect.set_fillclr(DarkCyan.alpha(100));
    _cursor_position = narrow_cast<long_t>(text().length());
    _text_display.create(Str{text()}, TextClr{Black}, _settings.font);
    
    set_clickable(true);
    set_scroll_enabled(true);
    set_scroll_limits(Rangef(0, 0), Rangef(0, 0));
    
    add_event_handler(HOVER, [this](Event e) {
        if(pressed())
            this->move_cursor(e.hover.x);
        else
            this->set_dirty();
    });
    add_event_handler(MBUTTON, [this](Event e) {
        if(e.mbutton.button != 0)
            return;
        
        if(e.mbutton.pressed) {
            this->move_cursor(e.mbutton.x);
            _selection_start = _cursor_position;
            _selection = lrange(_cursor_position, _cursor_position);
        } else
            this->move_cursor(e.mbutton.x);
    });
    
    add_event_handler(TEXT_ENTERED, [this](Event e) {
        if(read_only()) return;
        stage()->do_hover(NULL);
        if(this->onEnter(e)) {
        }
    });
    add_event_handler(SELECT, [this](auto) { this->set_dirty(); });
    
    add_event_handler(KEY, [this](Event e) {
        auto && [system, alt] = this->system_alt();
        if(read_only() && (e.key.code != Keyboard::C || !system)) return;
        if(e.key.pressed) {
            if(this->onControlKey(e)) {
                stage()->do_hover(NULL);
            }
        }
    });
}

Textfield::~Textfield() {
    if(_placeholder)
        delete _placeholder;
}

    void Textfield::set_text(const std::string &text) {
        if(text == _settings.text)
            return;
        
        _settings.text = text;
        _cursor_position = narrow_cast<long_t>(text.length());
        _text_offset = 0;
        _selection = lrange(-1, -1);
        
        set_content_changed(true);
    }
    
    void Textfield::enter() {
        if(_valid) {
            if(stage())
                stage()->select(NULL);
            if(_settings.on_enter)
                _settings.on_enter();
        }
    }
    
    bool Textfield::onControlKey(gui::Event e) {
        constexpr const char* alphanumeric = "abcdefghijklmnopqrstuvwxyz0123456789";
        
        auto && [system, alt] = system_alt();
        
        switch (e.key.code) {
            case Keyboard::Tab:
                if(_settings.on_tab)
                    _settings.on_tab();
                break;
            case Keyboard::Left:
                if(_cursor_position > 0) {
                    auto before = _cursor_position;
                    
                    if(system)
                        _cursor_position = 0;
                    else if(alt) {
                        // find the first word
                        auto k = find_first_of_reverse(utils::lowercase(text()), before, alphanumeric);
                        if(k == std::string::npos)
                            k = 0;
                        
                        _cursor_position = narrow_cast<long_t>(k);
                    }
                    else
                        _cursor_position--;
                    
                    if(e.key.shift) {
                        if(_selection.empty())
                            _selection_start = before;
                        if(_selection_start < _cursor_position)
                            _selection = lrange(_selection_start, _cursor_position);
                        else
                            _selection = lrange(_cursor_position, _selection_start);
                    } else
                        _selection = lrange(-1, -1);
                    
                    set_content_changed(true);
                    
                } else if(!e.key.shift && !_selection.empty()) {
                    _selection = lrange(-1, -1);
                    set_content_changed(true);
                }
                return true;
                
            case Keyboard::Right:
                if(sign_cast<size_t>(_cursor_position) < text().length()) {
                    auto before = _cursor_position;
                    
                    if(system)
                        _cursor_position = narrow_cast<long_t>(text().length());
                    else if(alt) {
                        // find the first word
                        auto k = utils::lowercase(text()).find_first_of(alphanumeric, before);
                        if(k != std::string::npos) {
                            // find the end of the word
                            k = utils::lowercase(text()).find_first_not_of(alphanumeric, k);
                            if(k == std::string::npos) // not found? jumpt to eof
                                k = text().length();
                            
                        } else
                            k = text().length();
                        
                        _cursor_position = narrow_cast<long_t>(k);
                    }
                    else
                        _cursor_position++;
                    
                    if(e.key.shift) {
                        if(_selection.empty())
                            _selection_start = before;
                        if(_selection_start < _cursor_position)
                            _selection = lrange(_selection_start, _cursor_position);
                        else
                            _selection = lrange(_cursor_position, _selection_start);
                    } else
                        _selection = lrange(-1, -1);
                    
                    set_content_changed(true);
                    
                } else if(!e.key.shift && !_selection.empty()) {
                    _selection = lrange(-1, -1);
                    set_content_changed(true);
                }
                return true;
                
            case Keyboard::A:
                if(system) {
                    _selection = lrange(0, narrow_cast<long_t>(text().length()));
                    _cursor_position = _selection.last;
                    set_content_changed(true);
                    return true;
                }
                break;
                
            case Keyboard::C:
                if(system) {
                    if(!_selection.empty()) {
                        auto sub = text().substr(_selection.first, _selection.last - _selection.first);
                        Print("Copying ", sub);
                        set_clipboard(sub);
                    } else {
                        Print("Copying ", text());
                        set_clipboard(text());
                    }
                    return true;
                }
                break;
                
            case Keyboard::V:
                if(system) {
                    std::string paste = get_clipboard();
                    if(!paste.empty()) {
                        Print("Pasting ", paste);
                        
                        std::string copy = text();
                        auto before = _cursor_position;
                        
                        if(!_selection.empty()) {
                            copy.erase(copy.begin() + _selection.first, copy.begin() + _selection.last);
                            _cursor_position = _selection.first;
                        }
                        
                        copy.insert(copy.begin() + _cursor_position, paste.begin(), paste.end());
                        _cursor_position += paste.length();
                        
                        if(isTextValid(copy, 8, _cursor_position)) {
                            _selection = lrange(-1, -1);
                            if(text() != copy) {
                                _settings.text = copy;
                                if(_settings.on_text_changed)
                                    _settings.on_text_changed();
                            }
                            
                        } else {
                            _cursor_position = before;
                        }
                        
                        set_content_changed(true);
                    }
                }
                return true;
                
            case Keyboard::BackSpace: {
                std::string copy = text();
                auto before = _cursor_position;
                
                if(!_selection.empty()) {
                    copy.erase(copy.begin() + _selection.first, copy.begin() + _selection.last);
                    _cursor_position = _selection.first;
                }
                else if(_cursor_position>0) {
                    _cursor_position--;
                    copy.erase(copy.begin() + _cursor_position);
                }
                
                if(isTextValid(copy, 8, _cursor_position)) {
                    _selection = lrange(-1, -1);
                    if(text() != copy) {
                        _settings.text = copy;
                        if(_settings.on_text_changed)
                            _settings.on_text_changed();
                    }
                    
                } else {
                    _cursor_position = before;
                }
                
                set_content_changed(true);
                return true;
            }
                
            case Keyboard::Return:
                enter();
                return true;
                
            default:
                break;
        }
        
        return false;
    }
    
    bool Textfield::onEnter(gui::Event e) {
        std::string k = std::string()+e.text.c;
        if(e.text.c == 8)
            return true;
        if(e.text.c < 10)
            return false;
        
        switch (e.text.c) {
            case '\n':
            case '\r':
            case 8:
                return true;
            case 27:
                if(parent() && parent()->stage())
                    parent()->stage()->select(NULL);
                return true;
                
            default: {
                std::string copy = text();
                auto before = _cursor_position;
                
                if(!_selection.empty()) {
                    copy.erase(copy.begin() + _selection.first, copy.begin() + _selection.last);
                    _cursor_position = _selection.first;
                }
                copy.insert(_cursor_position, k);
                _cursor_position++;
                _display_text_len++;
                
                if(isTextValid(copy, e.text.c, _cursor_position-1)) {
                    _settings.text = copy;
                    _selection = lrange(_cursor_position, _cursor_position);
                    
                } else {
                    _cursor_position = before;
                }
                
                if(_settings.on_text_changed)
                    _settings.on_text_changed();
                set_content_changed(true);
                
                return true;
            }
        }
        
        return false;
    }

void Textfield::set_text_color(const Color &c) {
    if(c == _settings.text_color)
        return;
    
    _settings.text_color = c;
    _cursor.set_fillclr(c);
    
    set_content_changed(true);
}

void Textfield::set_font(Font font) {
    font.align = Align::Left;
    
    if(_settings.font == font)
        return;
    
    _settings.font = font;
    _text_display.set_font(font);
    if(_placeholder)
        _placeholder->set(font);
    
    set_content_changed(true);
}

void Textfield::set_fill_color(const Color &c) {
    if(c == _settings.fill_color)
        return;
    
    _settings.fill_color = c;
    set_content_changed(true);
}

void Textfield::set_line_color(const Color &c) {
    if(c == _settings.line_color)
        return;
    
    _settings.line_color = c;
    set_content_changed(true);
}

void Textfield::set_postfix(const std::string &p) {
    if(p == _settings.postfix)
        return;
    
    _settings.postfix = p;
    set_content_changed(true);
}
    
    void Textfield::update() {
        auto ctx = OpenContext();
        
        static constexpr const Color BrightRed(255,150,150,255);
        Color base_color   = fill_color(),
              border_color = line_color();
        
        if(!valid())
            base_color = BrightRed.alpha(210);
        if(read_only()) {
            base_color = base_color.exposure(0.9);
            _text_display.set_color(DarkGray);
        } else
            _text_display.set_color(text_color());
        
        if(hovered())
            base_color = base_color.alpha(255);
        else if(selected())
            base_color = base_color.alpha(230);
        
        set_background(base_color, border_color);
        
        if(content_changed()) {
            // assumes test_text is only gonna be used in one thread at a time
            Timer timer;
            const Float2_t max_w = width() - margin * 2; // maximal displayed text width
            
            //Vec2 scale = stage_scale();
            //Vec2 real_scale = Drawable::real_scale();
            auto real_scale = this;
            
            if(sign_cast<size_t>(_cursor_position) > text().length())
                _cursor_position = narrow_cast<long_t>(text().length());
            if(_text_offset > text().length())
                _text_offset = text().length();
            
            auto r = Base::default_text_bounds(text(), real_scale, font());
            const Float2_t cursor_y = (height() - Base::default_line_spacing(font()))*0.5;
            
            if(_text_offset >= sign_cast<size_t>(_cursor_position))
                _text_offset = (size_t)max(0, _cursor_position-1);
            std::string before = text().substr(_text_offset, _cursor_position - _text_offset);
            
            r = Base::default_text_bounds(before, real_scale, font());
            
            if(_display_text_len < sign_cast<size_t>(_cursor_position))
                _display_text_len = _cursor_position;
            
            while(r.width >= max_w
                  && before.length() > 0
                  && _text_offset < sign_cast<size_t>(_cursor_position))
            {
                _text_offset++;
                before = before.substr(1);
                r = Base::default_text_bounds(before, real_scale, font());
            }
            
            // check whether after string is too short
            std::string after = text().substr(_cursor_position, _display_text_len - _cursor_position);
            if(after.length() < 2
               && sign_cast<size_t>(_cursor_position) < text().length()
               && after.length() < text().length() - _cursor_position)
            {
                _text_offset++; _display_text_len++;
                before = before.substr(1);
                after = text().substr(_cursor_position, _display_text_len - _cursor_position);
            }
            
            r = Base::default_text_bounds(before + after, real_scale, font());
            
            // maximize after string
            while(r.width < max_w && _display_text_len < text().length()) {
                _display_text_len++;
                after = text().substr(_cursor_position, _display_text_len - _cursor_position);
                r = Base::default_text_bounds(before + after, real_scale, font());
            }
            
            // limit after string
            /*while(r.width >= max_w && after.length() > 0) {
                after = after.substr(0, after.length()-1);
                r = SFBase::Base::default_text_bounds(before + after, scale, _font);
            }*/
            
            // ensure that the string displayed is long enough (otherwise the first character will be hidden, if another one is inserted right after it)
            while(r.width < max_w * 0.75 && _text_offset > 0) {
                _text_offset--;
                before = text().substr(_cursor_position - before.length() - 1, before.length() + 1);
                r = Base::default_text_bounds(before + after, real_scale, font());
            }
            
            while(r.width >= max_w + 5 && after.length() > 0) {
                after = after.substr(0, after.length()-1);
                r = Base::default_text_bounds(before + after, real_scale, font());
            }
            
            r = Base::default_text_bounds(before, real_scale, font());
            _cursor.set_bounds(Bounds(
                  Vec2(r.width + r.x + margin, cursor_y),
                  Size2(2, Base::default_line_spacing(font()))
                ));
            
            _display_text_len = after.length() + _cursor_position;
            
            _text_display.set_txt(before + after);
            _text_display.set_pos(Vec2(margin, cursor_y));
            
            /*
             * Determine selection size / position and set the selection rectangle.
             */
            if(!_selection.empty() && ((long)_text_offset < _selection.last))
            {
                // determine visible starting position
                Float2_t sx0;
                
                if((long)_cursor_position == _selection.first) {
                    sx0 = _cursor.pos().x;
                    
                } else {
                    sx0 = _text_display.pos().x;
                    
                    if((long)_text_offset < _selection.first) {
                        size_t visible_index = max((size_t)_selection.first, _text_offset);
                        std::string visible_not_selected = text().substr(_text_offset, visible_index - _text_offset);
                        r = Base::default_text_bounds(visible_not_selected, real_scale, font());
                        sx0 += r.width + r.x;
                    }
                }
                
                std::string visible_selected_text = text().substr(_text_offset, min(min(text().length(), _display_text_len + 1), (size_t)_selection.last) - _text_offset);
                
                // see how long the visible text is
                r = Base::default_text_bounds(visible_selected_text, real_scale, font());
                
                Float2_t sx1 = r.width + margin + 1;
                if((long)_cursor_position == _selection.last) {
                    sx1 = _cursor.pos().x;
                }
                
                // set boundaries
                _selection_rect.set_bounds(Bounds(sx0, _cursor.pos().y, min(width() - sx0 - margin, sx1 - sx0), _cursor.height()));
            }
        }
        
        advance_wrap(_text_display);
        
        if(text().empty() && _placeholder && !selected()) {
            _placeholder->set_pos(_text_display.pos());
            advance_wrap(*_placeholder);
           
        } else if(!postfix().empty()) {
            //auto tmp = new Text(_postfix, Vec2(width() - 5, height() * 0.5), _text_color.exposure(0.5), Font(max(0.1, _text_display.font().size * 0.9)));
            //tmp->set_origin(Vec2(1, 0.5));
            add<Text>(Str{postfix()},
                      Loc(width() - 5, height() * 0.5),
                      TextClr{text_color().exposure(0.5)},
                      Font(max(0.1, _text_display.font().size * 0.9)),
                      Origin(1, 0.5));
            //advance(tmp);
        }
            
        if(read_only()) {
            //auto tmp = new Text("(read-only)", Vec2(width() - 5, height() * 0.5), Gray, Font(max(0.1, _text_display.font().size * 0.9)));
            //tmp->set_origin(Vec2(1, 0.5));
            //advance(tmp);
            
            add<Text>(Str("(read-only)"), Loc(width() - 5, height() * 0.5), TextClr(Gray), Font(max(0.1, _text_display.font().size * 0.9)), Origin(1, 0.5));
        }
        
        if(!_selection.empty()) {
            advance_wrap(_selection_rect);
        }
        
        if(selected() && !read_only())
            advance_wrap(_cursor);
    }
    
    void Textfield::move_cursor(Float2_t mx) {
        std::string display = text().substr(_text_offset, _display_text_len - _text_offset);
        Float2_t x = 0;
        long idx = 0;
        
        const Float2_t character_size = round(25_F * font().size);
        while (x + character_size*0.5_F < mx
               && idx <= long(display.length()))
        {
            auto r = Base::default_text_bounds(display.substr(0, (size_t)idx++), this, font());
            x = r.width + r.x;
        }
        
        _cursor_position = narrow_cast<long_t>(_text_offset + max(0, idx - 1));
        
        if(pressed()) {
            if(_cursor_position < _selection_start) {
                _selection = lrange(_cursor_position, _selection_start);
            } else
                _selection = lrange(_selection_start, _cursor_position);
        }
        
        set_content_changed(true);
    }
    
    void Textfield::set_placeholder(const std::string &text) {
        if(!text.empty()) {
            if(_placeholder && _placeholder->text() == text)
                return;
            
            if(!_placeholder)
                _placeholder = new StaticText(Str{text}, TextClr{Gray}, _settings.font, Margins{0,0,0,0});
            else
                _placeholder->set_txt(text);
            
        } else {
            if(text.empty() && !_placeholder)
                return;
            
            if(_placeholder) {
                delete _placeholder;
                _placeholder = NULL;
            }
        }
        
        set_content_changed(true);
    }
}
