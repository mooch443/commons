#include "DrawStructure.h"
#include "DrawSFBase.h"
#include <gui/types/StaticText.h>
#include <gui/types/Button.h>
#include <misc/GlobalSettings.h>
#include <gui/Passthrough.h>

namespace cmn::gui {
    struct ErrorMessage {
        std::chrono::time_point<std::chrono::system_clock> time, last;
        std::string msg;
        gui::Color clr;
        
        Float2_t alpha;
        
        ErrorMessage() : time(std::chrono::system_clock::now()), last(time), alpha(1.0f) {}
        bool active() const {
            std::chrono::duration<Float2_t> d = std::chrono::system_clock::now() - time;
            auto d_ = std::chrono::duration_cast<std::chrono::milliseconds>(d).count() / 1000_F;
            if(d_ > 5) {
                return false;
            }
            return true;
        }
        void update() {
            if(!active()) {
                std::chrono::duration<Float2_t> d = std::chrono::system_clock::now() - last;
                auto d_ = std::chrono::duration_cast<std::chrono::milliseconds>(d).count() / 1000_F;
                
                alpha -= 0.25_F * d_;
                clr.a = (uint8_t)saturate(255_F * alpha);
            }
            
            last = std::chrono::system_clock::now();
        }
    };

    //! Securing usage of error_messages
    static auto error_message_lock = new std::recursive_mutex;

    //! Saves recent error messages for display
    static auto error_messages = new std::vector<ErrorMessage>;

    //! the errorlog debug callback to be released
    static void* debug_callback = nullptr;

    void deinit_errorlog() {
        set_debug_callback(nullptr);
        //throw std::invalid_argument("implement.");
        //if(debug_callback)
        //    DEBUG::UnsetDebugCallback(debug_callback);
        debug_callback = nullptr;
    }

    void init_errorlog() {
        //!TODO: Error log not implemented
        debug_callback = set_debug_callback([](PrefixLiterals::Prefix type, const std::string& msg, bool force_callback)
        {
            std::lock_guard lock(*error_message_lock);
            ErrorMessage obj;
            obj.msg = msg;
            switch (type) {
                case PrefixLiterals::EXCEPT:
                case PrefixLiterals::ERROR_PREFIX:
                    obj.clr = Red;
                    break;
                    
                case PrefixLiterals::WARNING:
                    obj.clr = Yellow;
                    break;
                case PrefixLiterals::INFO:
                    if(!force_callback)
                        return;
                    obj.clr = gui::Color(150, 225, 255, 255);
                    break;
                    
                default:
                    obj.clr = White;
                    break;
            }
            
            if(error_messages->size()+1 >= 10)
                error_messages->erase(error_messages->begin());
            error_messages->push_back(obj);
        });
    }

    void DrawStructure::draw_log_messages(Bounds screen) {
        if (screen.empty()) {
            auto scale = this->scale().reciprocal();
            //cmn::Print("scale: ", scale, " dims: ", Size2(this->width(), this->height()));
            //scale = Vec2(1);
            auto dim = Size2(this->width(), this->height()).mul(scale * gui::interface_scale());
            screen = Bounds(Vec2(), dim);
        }

        SectionGuard guard(*this, "log_messages()");
        //guard._section->set_scale(scale().reciprocal());
        
        {
            std::lock_guard lock(*error_message_lock);
            
            Vec2 pos = screen.pos() + Vec2(screen.width - 10, 0);
            for (size_t i=min(size_t(20), error_messages->size()); i>0; i--) {
                auto &e = error_messages->at(i-1);
                
                const size_t max_chars = 150;
                if(e.msg.length() > max_chars) {
                    for(size_t i=0; i<e.msg.length();) {
                        size_t next = i;
                        for (size_t j=min(e.msg.length()-1, i+max_chars); j>i+max_chars/2; --j) {
                            if(e.msg.at(j) == '-' || e.msg.at(j) == ' ') {
                                next = j;
                                break;
                            }
                        }
                        
                        if(next <= i) {
                            next += max_chars;
                        }
                        
                        auto sub = e.msg.substr(i, min(e.msg.length() - i, next - i));
                        auto t = text(Str{sub}, Loc(pos), TextClr{e.clr}, Font(0.5f, Align::Right), Scale(scale().reciprocal()));
                        pos.y += t->global_bounds().height;
                        
                        i = next;
                    }
                    
                } else {
                    auto t = text(Str{e.msg}, Loc(pos), TextClr{e.clr}, Font(0.5f, Align::Right), Scale(scale().reciprocal()));
                    pos.y += t->global_bounds().height;
                }
                
                e.update();
                
                if(e.alpha <= 0)
                    error_messages->erase(error_messages->begin() + int64_t(i - 1));
            }
        }
    }

    void Dialog::set_custom_element(derived_ptr<Entangled> &&e) {
        _custom = std::move(e);
        this->set_dirty();
    }

void Dialog::set_closed() {
    _closed = true;
    if(parent()) {
        parent()->set_dirty();
        if(parent()->stage()) {
            parent()->stage()->hovered_object() = nullptr;
            parent()->stage()->selected_object() = nullptr;
            parent()->stage()->set_dirty(nullptr);
        }
    }
}
    
    Dialog::Dialog(DrawStructure& d, std::function<bool(Result)>&& callback, const std::string &text, const std::string& title, const std::string& okay, const std::string& abort, const std::string& second, const std::string& third, const std::string& fourth)
      : _closed(false),
        _graph(d),
        _title_bg(FillClr{White.alpha(100)}),
        _text(new StaticText(attr::Str(text), Loc(250, 140), SizeLimit(700, 500), Font(0.8f))),
        _title(new StaticText(attr::Str(title), SizeLimit(700, 0), Font(0.9f, Style::Bold), Margins{0, -5, 0, 5})),
        _okay(Button::MakePtr(attr::Str(okay))),
        _abort(abort.empty() ? nullptr : Button::MakePtr(attr::Str(abort))),
        _second(second.empty() ? nullptr : Button::MakePtr(attr::Str(second))),
        _third(third.empty() ? nullptr : Button::MakePtr(attr::Str(third))),
        _fourth(fourth.empty() ? nullptr : Button::MakePtr(attr::Str(fourth))),
        _buttons(new HorizontalLayout),
        _layout(std::vector<Layout::Ptr>{_text, _buttons}),
        _callback(std::move(callback))
    {
        set_name("Dialog");
        _layout.set(Margins(5, 5, 5, 5));
        
        const Float2_t margin = 20;
        _okay->set_size(Size2(_okay->text_dims().width + margin, 40));
        //_okay->set_size(Size2(gui::Base::default_text_bounds(strip_html(_okay->txt()), nullptr, _okay->font()).width + 20, 40));
        _okay->set_fill_clr(Color::blend(DarkCyan.exposure(0.5).alpha(110), Green.exposure(0.15)));
        if (_abort) {
            _abort->set_size(Size2(_abort->text_dims().width + margin, 40));
            //_abort->set_size(Size2(gui::Base::default_text_bounds(strip_html(_abort->txt()), nullptr, _abort->font()).width + 20, 40));
            _abort->set_fill_clr(Color::blend(DarkCyan.exposure(0.5).alpha(110), Red.exposure(0.2)));
        }
        
        if(_second)
            _second->set_size(Size2(_second->text_dims().width + margin, 40));
        //    _second->set_size(Size2(gui::Base::default_text_bounds(strip_html(_second->txt()), nullptr, _second->font()).width + 20, 40));
        if(_third)
            _third->set_size(Size2(_third->text_dims().width + margin, 40));
        //    _third->set_size(Size2(gui::Base::default_text_bounds(strip_html(_third->txt()), nullptr, _third->font()).width + 20, 40));
        if(_fourth)
            _fourth->set_size(Size2(_fourth->text_dims().width + margin, 40));
        //    _fourth->set_size(Size2(gui::Base::default_text_bounds(strip_html(_fourth->txt()), nullptr, _fourth->font()).width + 20, 40));

        _title_bg.set_bounds(Bounds(Vec2(5, 10), Size2(max(700, _layout.width() + 20), 60)));
        _layout.set_pos(_title_bg.pos() + Vec2(0, _title_bg.height()));
        _text->set_max_size(Size2(_title_bg.width() - 50, 0));
        
        std::vector<Layout::Ptr> buttons;
        if(_okay)
            buttons.push_back(_okay);
        if(_second)
            buttons.push_back(_second);
        if(_third)
            buttons.push_back(_third);
        if(_fourth)
            buttons.push_back(_fourth);
        if(_abort)
            buttons.push_back(_abort);
        _buttons->set_children(buttons);
        
        set(FillClr{DarkCyan.exposure(0.2f).alpha(220)});
        set(LineClr{Black});
        //set_background(DarkCyan.exposure(0.2f).alpha(220), Black);
        
        //_text->set_background(Transparent, Transparent);
        _text->reset_bg();
        _title->set_origin(Vec2(0.5));
        
        if(_abort) {
            _abort->on_click([this](auto) {
                _result = ABORT;
                if(_callback(_result)) {
                    set_closed();
                }
            });
        }
        
        set_clickable(true);
        add_event_handler(EventType::KEY, [this](Event e){
            if(e.key.pressed) {
                if(e.key.code == Codes::Return) {
                    _result = OKAY;
                    if(_callback(_result)) {
                        set_closed();
                    }
                } else if(e.key.code == Codes::Escape && _abort) {
                    _result = ABORT;
                    if(_callback(_result)) {
                        set_closed();
                    }
                }
            }
        });
        
        _okay->on_click([this](auto) {
            _result = OKAY;
            if(_callback(_result)) {
                set_closed();
            }
        });
        
        if(_second) {
            _second->on_click([this](auto) {
                _result = SECOND;
                if(_callback(_result)) {
                    set_closed();
                }
            });
        }
        
        if(_third) {
            _third->on_click([this](auto) {
                _result = THIRD;
                if(_callback(_result)) {
                    set_closed();
                }
            });
        }
        
        if(_fourth) {
            _fourth->on_click([this](auto) {
                _result = FOURTH;
                if(_callback(_result)) {
                    set_closed();
                }
            });
        }
    }

    void Dialog::update_sizes(DrawStructure& d) {
        Size2 size = Size2(d.width(), d.height());
        if(!d.dialog_window_size().empty()) {
            //cmn::Print("dialog ", d.dialog_window_size(), " vs. ", size);
            size = d.dialog_window_size();
        }
        
        //set_bounds(Bounds(Vec2(size*0.5), Size2(_title_bg.width() + 10,300)));
        //
        /*_text->set(LineClr{Blue});
        _text->set(FillClr{Red.alpha(50)});
        _layout.set(LineClr{Green});
        _buttons->set(LineClr{Purple});*/
        
        //_layout.update_layout();
        //_layout.update();
        //_text->update();
        
        //_layout.auto_size();
        //_buttons->auto_size();
        
        set(LineClr{0,0,0,200});
        set(FillClr{50,50,50,200});
        set_origin(Vec2(0.5));
        set_pos(size * 0.5);
        
        _layout.update();
        _layout.auto_size();
        if(_buttons)
            _buttons->auto_size();
        auto_size({5,10});
    }
    
    Dialog::~Dialog() {
        set_closed();
        /*delete _okay;
        if(_abort)
            delete _abort;
        if(_text)
            delete _text;*/
    }
    
    void Dialog::set_parent(SectionInterface* parent) {
        Entangled::set_parent(parent);
        
        if(parent && parent->stage()) {
            parent->stage()->select(this);
            update_sizes(*parent->stage());
        }
    }
    
    void Dialog::update() {
        if(_custom && _layout.children().size() == 2) {
            std::vector<Layout::Ptr> children{_text, _custom, _buttons};
            _layout.set_children(children);
            //_layout.auto_size();
        }
        
        OpenContext([this]{
            advance_wrap(_title_bg);
            //d.wrap_object(*_text);
            advance_wrap(*_title);
            advance_wrap(_layout);
        });
        
        update_sizes(_graph);
        
        _layout.set_origin(Vec2(0.5f, 0));
        _layout.set_pos(Vec2(0.5f * width(), _layout.pos().y));
        _title->set_pos(_title_bg.size() * 0.5f + Vec2(0, _title_bg.height() * 0.2f));
        
        _layout.set_policy(gui::VerticalLayout::Policy::CENTER);
        _buttons->set_policy(gui::HorizontalLayout::Policy::CENTER);
        
        //set_size(Size2(width(), _layout.height() + _layout.pos().y + 10));
        set_scale(_graph.scale().reciprocal() / gui::interface_scale());
                //d.wrap_object(*_okay);
        //if(_abort)
        //    d.wrap_object(*_abort);
    }
    
    DrawStructure::~DrawStructure() {
        //auto guard = GUI_LOCK(_lock);
        //cmn::Print("* Deselecting ", hex(_selected_object));
        _selected_object = _hovered_object = nullptr;
        _active_section = nullptr;
        _root.set_stage(NULL);
        clear();
    }

    void DrawStructure::update_dialogs() {
        if(!_dialogs.empty()) {
            Size2 size = Size2(width(), height());
            if(!dialog_window_size().empty())
                size = dialog_window_size();
            
            auto rect = new Rect(Box(Vec2(size) * 0.5, size), FillClr{Black.alpha(200)}, LineClr{Red}, Origin{0.5});
            rect->set_clickable(true);
            rect = add_object(rect);
            wrap_object(*_dialogs.front());
        }
        
        if(!_dialogs.empty() && _dialogs.front()->is_closed()) {
            _dialogs.pop_front();
            set_dirty(nullptr);
        }
    }

void DrawStructure::close_dialogs() {
    //auto guard = GUI_LOCK(lock());
    if(!_dialogs.empty() && _dialogs.front())
        _dialogs.front()->set_closed();
    _dialogs.clear();
    update_dialogs();
    set_dirty(nullptr);
}
    
    Dialog* DrawStructure::_dialog(std::function<bool(Dialog::Result)>&& callback, const std::string &text, const std::string& title, const std::string& okay, const std::string& abort, const std::string& second, const std::string& third, const std::string& fourth)
    {
        //auto guard = GUI_LOCK(_lock);
        auto d = new Dialog(*this, std::move(callback), text, title, okay, abort, second, third, fourth);
        d->set_scale(scale().reciprocal());
        _dialogs.emplace_back(d);
        set_dirty(nullptr);
        return d;
    }

    Dialog* DrawStructure::dialog(const std::string &text, const std::string& title, const std::string& okay, const std::string& abort, const std::string& second, const std::string& third, const std::string& fourth)
    {
        std::function<bool(Dialog::Result)> fn = [](Dialog::Result)->bool{return true;};
        return _dialog(std::move(fn), text, title, okay, abort, second, third, fourth);
    }
    
    Vertices* DrawStructure::vertices(const std::vector<Vec2> &points, const gui::Color &color, PrimitiveType type) {
        std::vector<Vertex> array;
        array.resize(points.size());
        for(size_t i=0; i<points.size(); i++)
            array[i] = Vertex(points[i], color);
        
        return static_cast<Vertices*>(add_object(new Vertices(array, type, Vertices::TRANSPORT)));
    }
    
    Vertices* DrawStructure::vertices(const std::vector<Vertex> &points, PrimitiveType type) {
        return static_cast<Vertices*>(add_object(new Vertices(points, type, Vertices::TRANSPORT)));
    }
    
    ExternalImage* DrawStructure::image(const Vec2 &pos, ExternalImage::Ptr&& image, const Vec2& scale, const Color& color) {
        if(!image) {
            FormatWarning("Trying to add image that is nullptr.");
            return NULL;
        }
        if(image->empty()) {
            FormatWarning("Trying to add image with dimensions ", image->cols,"x",image->rows,".");
            return NULL;
        }
        
        return create<Type::data::values::IMAGE, ExternalImage>(std::move(image), pos, scale, color);
        //return static_cast<ExternalImage*>(add_object(new ExternalImage(std::move(image), pos, scale, color)));
    }
    
    /*Drawable* DrawStructure::_add_object(gui::Drawable *ptr) {
        if(ptr->type() == Type::SECTION)
            throw U_EXCEPTION("Cannot add Section using add_object. Use wrap_object instead.");
        
        if(!_active_section)
            begin_section("root");
        
        return _active_section->add(ptr);
    }*/

    void DrawStructure::register_end_object(gui::Drawable& d) {
        std::unique_lock guard(_end_object_mutex);
        _end_objects.insert(&d);
    }

    void DrawStructure::unregister_end_object(gui::Drawable& d) {
        std::unique_lock guard(_end_object_mutex);
        auto it = _end_objects.find(&d);
        if(it != _end_objects.end())
            _end_objects.erase(it);
    }
    
    void DrawStructure::wrap_object(gui::Drawable& d) {
        if(!_active_section)
            begin_section("root");
        
        return _active_section->wrap_object(&d);
    }

    void DrawStructure::clear_hover() {
        do_hover(NULL);
    }
    
    void DrawStructure::update_hover() {
        // triggered by hovered drawable if its size is changed,
        // or if it is moved
        auto d = find(_mouse_position.x, _mouse_position.y);
        if(d != _hovered_object) {
            Event e(HOVER);
            e.hover.x = _mouse_position.x;
            e.hover.y = _mouse_position.y;
            e.hover.hovered = true;
            
            do_hover(d, e);
        }
    }
    
    bool DrawStructure::remove_wrapped(gui::Drawable *d) {
        return _root.remove_wrapped(d);
    }
    
    void DrawStructure::Print(const Base* base) {
        auto str = _root.toString(base);
        cmn::Print("String\n", str);
        cmn::Print("Length: ", collect().size());
    }
    
    std::vector<Drawable*> DrawStructure::collect() {
        _root.update_bounds();
        //update_hover();
        return _root.collect();
    }
    
    Section* DrawStructure::begin_section(const std::string& name, bool reuse) {
        if(!_active_section && name != "root") {
            push_section(&_root);
            //if(_root._index)
            //    _root.begin();
        }
        
        Section *s = NULL;
        if(_active_section && !(s = _active_section->find_section(name))) {
            if(&_root != _active_section
               && (s = _root.find_section(name)) != NULL)
                throw U_EXCEPTION("Cannot add section ",name," twice.");
            
            Section *section = new Section(this, _active_section, name);
            _active_section->add(section);
            s = section;
            
        } else {
            if(!s)
                s = &_root;
            if(contains(_sections, s))
                throw U_EXCEPTION("Cannot add section ",name," twice.");
            
            if(s->parent()) {
                assert(s->parent()->type() == Type::SECTION);
                auto parent = static_cast<Section*>(s->parent());
                
                auto it = std::find(parent->children().begin(), parent->children().end(), s);
                if(it != parent->children().end()) {
                    if(it != parent->children().begin() + (int64_t)parent->_index) {
                        if(parent->_index >= size_t(it - parent->children().begin())) {
                            parent->_index--;
                        }
                        parent->children().erase(it);
                        
                        parent->children().insert(parent->children().begin() + (int64_t)parent->_index, s);
#ifndef NDEBUG
                        cmn::Print("Moved section ",name," (",parent->_index,")");
#endif
                    }
                    
                    parent->_index++;
                }
            }
        }
        
        push_section(s, reuse);
        
        return s;
    }
    
    void DrawStructure::erase(gui::Drawable *d) {
        if(_selected_object 
           && (_selected_object == d || _selected_object->is_child_of(d)))
        {
            //cmn::Print("* Deselecting erase ", hex(d));
            _selected_object = nullptr;
        }
        
        if(_hovered_object 
           && (_hovered_object == d || _hovered_object->is_child_of(d)))
            _hovered_object = nullptr;
    }
    
    void DrawStructure::finalize_section(const std::string& name) {
        if(!_active_section)
            throw U_EXCEPTION("No sections to be ended (",name,").");
        
        pop_section();
    }
    
    void DrawStructure::finalize_section(const gui::Section *section) {
        UNUSED(section);
        assert(_active_section == section);
        pop_section();
    }
    
    Drawable* DrawStructure::find(Float2_t x, Float2_t y) {
        _root.update_bounds();
        results.clear();
        _root.find(x, y, results);
        
        int64_t Z = -1;
        Drawable* found = nullptr;
        for(auto ptr : results) {
            if(!found || ptr->z_index() > Z) {
                Z = ptr->z_index();
                found = ptr;
            }
        }
        return found;
    }
    
    Drawable* DrawStructure::mouse_move(Float2_t x, Float2_t y) {
        _mouse_position.x = x;
        _mouse_position.y = y;
        
        auto d = find(x, y);
        Event e(HOVER);
        e.hover.x = x;
        e.hover.y = y;
        e.hover.hovered = d != nullptr;
        
        do_hover(d, e);
        
        return d;
    }
    
    Drawable* DrawStructure::mouse_down(bool left_button) {
        mouse_state[left_button ? 0 : 1] = true;
        
        Float2_t x = _mouse_position.x, y = _mouse_position.y;
        auto d = find(x, y);
        
        Event e(HOVER);
        e.hover.x = x;
        e.hover.y = y;
        
        do_hover(d, e);
        select(d);
        
        if(d) {
            d->mdown(x, y, left_button); // ? dragging
        }
        
        return d;
    }
    
    Drawable* DrawStructure::mouse_up(bool left_button) {
        mouse_state[left_button ? 0 : 1] = false;
        
        if(_selected_object) {
            std::string type(_selected_object->type().name());
            if(dynamic_cast<HasName*>(_selected_object))
                type = dynamic_cast<HasName*>(_selected_object)->name();
            
            Float2_t x = _mouse_position.x, y = _mouse_position.y;
            _selected_object->mup(x, y, left_button);
        }
        
        return _selected_object;
    }
    
    bool DrawStructure::is_mouse_down(int button) const {
        if(button == 0)
            return mouse_state[0];
        else if(button == 1)
            return mouse_state[1];
        throw InvalidArgumentException("This is an invalid button.");
    }

    bool DrawStructure::event(Event e) {
        //auto lock = GUI_LOCK(_lock);
        Drawable* d = NULL;

        switch(e.type) {
            case MMOVE: {
                const Float2_t interface_scale = gui::interface_scale();
                
                Event hover(EventType::HOVER);
                hover.hover.x = e.move.x / scale().x * interface_scale;
                hover.hover.y = e.move.y / scale().y * interface_scale;
                
                d = mouse_move(hover.hover.x, hover.hover.y);
                set_dirty(NULL);
                
                if(selected_object()) {
                    Drawable *draggable = selected_object();
                    while (draggable && !draggable->draggable())
                        draggable = draggable->parent();
                    
                    if(draggable && draggable->pressed() && draggable->being_dragged()) {
                        auto pos = Vec2(hover.hover.x, hover.hover.y);
                        pos = draggable->global_transform().getInverse().transformPoint(pos);
                        
                        auto dg = draggable->relative_drag_start();
                        
                        if(draggable->draggable())
                            draggable->set_pos(draggable->pos() + pos - dg);
                        
                        auto it = draggable->_event_handlers.find(EventType::DRAG);
                        if(it != draggable->_event_handlers.end()) {
                            Event drag(EventType::DRAG);
                            drag.drag.x = draggable->pos().x;
                            drag.drag.y = draggable->pos().y;
                            drag.drag.rx = (pos - dg).x;
                            drag.drag.ry = (pos - dg).y;
                            for(auto &handler : it->second) {
                                (*handler)(drag);
                            }
                        }
                    }
                }
                break;
            }
            case MBUTTON:
                if(e.mbutton.pressed)
                    d = mouse_down(e.mbutton.button == 0);
                else
                    d = mouse_up(e.mbutton.button == 0);
                break;
            case KEY:
                if(e.key.pressed)
                    return key_down((Codes)e.key.code, e.key.shift);
                else
                    return key_up((Codes)e.key.code, e.key.shift);
                break;
            case TEXT_ENTERED:
                return text_entered(e.text.c);
                
            case SCROLL:
                d = _hovered_object;
#if __linux__ || WIN32
                e.scroll.dx *= 15;
                e.scroll.dy *= 15;
#endif
                scroll(Vec2(e.scroll.dx, e.scroll.dy));
                break;
            default:
                break;
        }

        return d != NULL;
    }
    
    void DrawStructure::select(Drawable* d) {
        if(d == _selected_object)
            return;
        
        assert(not d || d->is_child_of(&_root));
        Drawable * parent = NULL;
        Drawable * previous = _selected_object;
        //cmn::Print(" * instead of ", hex(previous), " now selecting ", hex(d));
        _selected_object = d;
        
        if(previous) {
            parent = previous->parent();
            previous->deselect();
            
            if(d && d->is_child_of(parent)) {
                // dont deselect
            } else {
                while(parent) {
                    parent->deselect();
                    parent = parent->parent();
                }
            }
        }
        
        if(d) {
            d->select();
        }
    }
    
    bool DrawStructure::key_down(Codes code, bool shift) {
        pressed_keys.insert(code);
        
        if(_selected_object) {
            Event e(KEY);
            e.key = {code, true, shift};
            return _selected_object->kdown(e);
        }
        
        return false;
    }
    
    bool DrawStructure::key_up(Codes code, bool shift) {
        if(pressed_keys.count(code))
            pressed_keys.erase(code);
        
        if(_selected_object) {
            Event e(KEY);
            e.key = {code, false, shift};
            return _selected_object->kup(e);
        }
        
        return false;
    }
    
    bool DrawStructure::is_key_pressed(Codes code) const {
        return pressed_keys.count(code);
    }
    
    bool DrawStructure::text_entered(char c) {
        if(_selected_object) {
            Event e(TEXT_ENTERED);
            e.text = {c};
            return _selected_object->text_entered(e);
        }
        
        return false;
    }
    
    void DrawStructure::scroll(const Vec2& delta) {
        if(_hovered_object) {
            Event e(SCROLL);
            e.scroll.dx = delta.x;
            e.scroll.dy = delta.y;
            _hovered_object->scroll(e);
            
            update_hover();
        }
    }
    
    Drawable* Section::find(const std::string& search) {
        if(HasName::name() == search)
            return this;
        
        return SectionInterface::find(search);
    }
    
    Drawable* DrawStructure::find(const std::string& name) {
        return _root.find(name);
    }
    
    void DrawStructure::all_changed() {
        std::queue<SectionInterface*> q;
        q.push(&_root);
        
        while(!q.empty()) {
            auto ptr = q.front();
            q.pop();
            
            ptr->set_bounds_changed();
            
            apply_to_objects(ptr->children(), [&q](auto c) {
                c->set_bounds_changed();
                
                if(c->type() == Type::SECTION || c->type() == Type::ENTANGLED) {
                    q.push(static_cast<SectionInterface*>(c));
                }
            });
        }
    }
    
    void DrawStructure::set_dirty(const Base* base) {
        if(!base) {
            _root.set_dirty();
            return;
        }
        
        auto cache = _root.cached(base);
        if(cache)
            cache->set_changed(true);
        else
            _root.insert_cache(base, std::make_unique<CacheObject>());
    }

    void DrawStructure::set_size(const Size2& size) {
        _width = narrow_cast<uint16_t>(size.width);
        _height = narrow_cast<uint16_t>(size.height);
    }

Size2 DrawStructure::dialog_window_size() const {
    return _dialog_window_size;
}

void DrawStructure::set_dialog_window_size(const Size2 &s) {
    _dialog_window_size = s;
}

Vec2 DrawStructure::scale() const {
    return _scale;
}

}
