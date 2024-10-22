#include "Combobox.h"
#include <gui/LabeledField.h>
#include <gui/ParseLayoutTypes.h>

namespace cmn::gui {
using namespace dyn;

tl::expected<std::string, const char*> default_value_of(std::string_view name) {
    if(GlobalSettings::current_defaults().has(name)) {
        return GlobalSettings::current_defaults().at(name).get().valueString();
    } else if(GlobalSettings::defaults().has(name)) {
        return GlobalSettings::defaults().at(name).get().valueString();
    }
    
    return tl::unexpected("No default value available.");
}

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
    
    _reset_button = std::make_shared<Button>(Str{"<sym>⮌</sym>"}, Size{25,28});
    _reset_button->on_click([this](auto){
        if(not _does_not_equal_default)
            return;
        
        auto name = parameter();
        if(name.empty())
            return;
        
        if(stage())
            stage()->do_hover(nullptr);
        
        if(GlobalSettings::current_defaults().has(name)) {
            GlobalSettings::current_defaults().at(name).get().copy_to(settings_map());
        } else if(GlobalSettings::defaults().has(name)) {
            GlobalSettings::defaults().at(name).get().copy_to(settings_map());
        } else {
            FormatWarning("Do not have a default value for ", name);
            return;
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

void Combobox::update_defaults() {
    auto b = _does_not_equal_default;
    _does_not_equal_default = false;
    
    if(auto name = parameter();
       _value && not name.empty() && settings_map().has(name))
    {
        auto value = settings_map().at(name).get().valueString();
        if(auto def = default_value_of(name);
           def && value != def.value())
        {
            _does_not_equal_default = true;
        }
    }
    
    if(b != _does_not_equal_default) {
        auto s = _settings.bounds.size();
        _settings.bounds << Size2();
        set_size(s);
    }
}

void Combobox::update() {
    if(not content_changed())
        return;
    
    std::vector<Layout::Ptr> objects{
        _dropdown
    };
    
    if(_value)
        _value->add_to(objects);
    
    update_defaults();
    if(_does_not_equal_default) {
        //Print("Value ", _value->text()->txt(), " != ", def.value());
        objects.push_back(_reset_button);
    }
    
    _layout.set_children(objects);
    
    OpenContext([this](){
        advance_wrap(_layout);
    });
    
    _layout.update_layout();
    _layout.auto_size();
    //Entangled::set_size({_layout.width(), height()});
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
        
        if(stage() && stage()->hovered_object() == this) {
            stage()->do_hover(nullptr);
        }
        
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
        {
            Font center{font};
            center.align = Align::Center;
            _reset_button->set(center);
        }
        set_content_changed(true);
    }
}
void Combobox::set(attr::FillClr clr) {
    if(_settings.fill_clr != clr) {
        _settings.fill_clr = clr;
        if(_value)
            _value->set(clr);
        _dropdown->set(clr);
        _reset_button->set(FillClr{Color::blend(clr, Yellow.alpha(50))});
        set_content_changed(true);
    }
}
void Combobox::set(attr::LineClr clr) {
    if(_settings.line_clr != clr) {
        _settings.line_clr = clr;
        if(_value)
            _value->set(clr);
        _dropdown->set(clr);
        _reset_button->set(clr);
        set_content_changed(true);
    }
}
void Combobox::set(attr::TextClr clr) {
    if(_settings.text_clr != clr) {
        _settings.text_clr = clr;
        if(_value)
            _value->set(clr);
        _dropdown->set(clr);
        _reset_button->set(clr);
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
        update_value();
        _reset_button->set(attr::Size(25, p.height));
        _dropdown->set(attr::Size{p.width * 0.35f - (_does_not_equal_default ? _reset_button->width() - 5_F * 2_F : 0_F), p.height});
        set_content_changed(true);
    }
    Entangled::set_size(p);
}

void Combobox::update_value() {
    update_defaults();
    
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
