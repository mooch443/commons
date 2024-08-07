#include "Combobox.h"
#include <gui/LabeledField.h>
#include <gui/ParseLayoutTypes.h>

namespace cmn::gui {
using namespace dyn;

void Combobox::init() {
    _dropdown = std::make_shared<Dropdown>(Box(0, 0, 800, 28));
    auto keys = settings_map().keys();
    _dropdown->set_items(std::vector<Dropdown::TextItem>(keys.begin(), keys.end()));
    _dropdown->on_select([this](auto, const Dropdown::TextItem & item){
        auto name = item.name();
        if(settings_map().has(name)) {
            set(ParmName{name});
            if(_settings.on_select)
                _settings.on_select(ParmName{name});
        }
        _dropdown->set_opened(false);
        if(_value) {
            auto ptr = _value->representative().get();
            if(ptr->parent() && ptr->parent()->stage())
                ptr->parent()->stage()->select(ptr);
        }
    });
    
    _layout.set(Margins{0, 0, 0, 0});
    _layout.set_policy(HorizontalLayout::Policy::TOP);
    _layout.auto_size();
    //set_background(_settings.fill_clr, _settings.line_clr);
    set_bounds(_settings.bounds);
    _layout.set_clickable(true);
}

std::optional<Dropdown::TextItem> Combobox::last_hovered_item() const {
    if(not _dropdown)
        return std::nullopt;
    return _dropdown->currently_hovered_item();
}

void Combobox::update() {
    if(not content_changed())
        return;
    
    std::vector<Layout::Ptr> objects{
        _dropdown
    };
    
    if(_value)
        _value->add_to(objects);
    _layout.set_children(objects);
    
    {
        begin();
        advance_wrap(_layout);
        end();
    }
    
    _layout.update_layout();
    _layout.auto_size();
    Entangled::set_size({_layout.width(), height()});
}

void Combobox::set(ListDims_t dims) {
    if(_dropdown)
        _dropdown->list().set(dims);
}

void Combobox::set(Combobox::OnSelect_t on_select) {
    _settings.on_select = on_select;
}

void Combobox::set(LabelDims_t dims) {
    if(_dropdown)
        _dropdown->list().set(dims);
}

void Combobox::set(ParmName name) {
    if(name != _settings.param) {
        _settings.param = name;
        
        _value = LabeledField::Make(_gui, name);
        if(not dynamic_cast<const LabeledCheckbox*>(_value.get())) {
            _value->set_description("");
        }
        
        update_value();
        /*_value->set(attr::Size{
            _settings.bounds.width * 0.65f,
            _settings.bounds.height
        });*/
        //_value->set(_settings.content);
        
        set_content_changed(true);
    }
}

void Combobox::set(attr::Font font)   {
    if(_settings.font != font) {
        _settings.font = font;
        if(_value)
            _value->set(font);
        _dropdown->set(font);
        set_content_changed(true);
    }
}
void Combobox::set(attr::FillClr clr) {
    if(_settings.fill_clr != clr) {
        _settings.fill_clr = clr;
        if(_value)
            _value->set(clr);
        _dropdown->set(clr);
        set_content_changed(true);
    }
}
void Combobox::set(attr::LineClr clr) {
    if(_settings.line_clr != clr) {
        _settings.line_clr = clr;
        if(_value)
            _value->set(clr);
        _dropdown->set(clr);
        set_content_changed(true);
    }
}
void Combobox::set(attr::TextClr clr) {
    if(_settings.text_clr != clr) {
        _settings.text_clr = clr;
        if(_value)
            _value->set(clr);
        _dropdown->set(clr);
        set_content_changed(true);
    }
}

void Combobox::set(ListLineClr_t clr) {
    if(_dropdown)
        _dropdown->list().set(clr);
    if(_value)
        _value->set(clr);
}
void Combobox::set(ListFillClr_t clr) {
    if(_dropdown)
        _dropdown->list().set(clr);
    if(_value)
        _value->set(clr);
}
void Combobox::set(LabelBorderColor_t clr) {
    if(_dropdown)
        _dropdown->list().set(clr);
    if(_value)
        _value->set(clr);
}
void Combobox::set(LabelColor_t clr) {
    if(_dropdown)
        _dropdown->list().set(clr);
    if(_value)
        _value->set(clr);
}
void Combobox::set(attr::Str content) {
    if(_settings.content != content) {
        _settings.content = content;
        if(_value)
            _value->set(content);
        set_content_changed(true);
    }
}
void Combobox::set(Placeholder_t p) {
    if(_dropdown && _dropdown->textfield())
        _dropdown->textfield()->set(p);
}
void Combobox::set(attr::Prefix prefix) {
    if(_settings.prefix != prefix) {
        _settings.prefix = prefix;
        if(_value)
            _value->set(prefix);
        set_content_changed(true);
    }
}
void Combobox::set(attr::Postfix postfix) {
    if(_settings.postfix != postfix) {
        _settings.postfix = postfix;
        if(_value)
            _value->set(postfix);
        set_content_changed(true);
    }
}
void Combobox::set(attr::SizeLimit limit) {
    if(_settings.sizeLimit != limit) {
        _settings.sizeLimit = limit;
        if(_value)
            _value->set(limit);
        set_content_changed(true);
    }
}
void Combobox::set(attr::HighlightClr clr) {
    if(_settings.highlight != clr) {
        _settings.highlight = clr;
        if(_value)
            _value->set(clr);
        set_content_changed(true);
    }
}

ParmName Combobox::parameter() const {
    return ParmName(_settings.param);
}

void Combobox::set_bounds(const Bounds& bds) {
    if(not _settings.bounds.Equals(bds)) {
        _settings.bounds = bds;
        set_size(bds.size());
        /*_dropdown->set(attr::Size(bds.width * 0.35f, bds.height));
        if(_value)
            _value->set(attr::Size(bds.width * 0.65f, bds.height));*/
        set_content_changed(true);
    }
    Entangled::set_bounds(bds);
}

void Combobox::set_pos(const Vec2& p) {
    if(not _settings.bounds.pos().Equals(p)) {
        _settings.bounds << p;
        set_content_changed(true);
    }
    Entangled::set_pos(p);
}

void Combobox::set_size(const Size2& p) {
    if(not _settings.bounds.size().Equals(p)) {
        _settings.bounds << p;
        _dropdown->set(attr::Size{p.width * 0.35f, p.height});
        update_value();
        set_content_changed(true);
    }
    Entangled::set_size(p);
}

void Combobox::update_value() {
    if(not _value)
        return;
    
    _value->set(attr::Size(_settings.bounds.width * 0.65f, _settings.bounds.height));
    
    _value->set(ListDims_t{_settings.bounds.width * 0.65f, 300});
    _value->set(LabelDims_t{_settings.bounds.width * 0.65f, _settings.bounds.height});
    
    _value->set(_settings.fill_clr);
    _value->set(_settings.line_clr);
    _value->set(_settings.text_clr);
    _value->set(_settings.font);
    _value->set(_settings.highlight);
    _value->set(_settings.sizeLimit);
    if(_dropdown) {
        _value->set(ListLineClr_t{_dropdown->list().list_line_clr()});
        _value->set(ListFillClr_t{_dropdown->list().list_fill_clr()});
    }
    _value->set(LabelColor_t{(Color)_settings.fill_clr});
    _value->set(LabelBorderColor_t{(Color)_settings.line_clr});
    _value->set(_settings.postfix);
    _value->set(_settings.prefix);
}

}
