#include "LabeledField.h"
#include <misc/GlobalSettings.h>

namespace gui::dyn {

namespace settings_scene {
GlobalSettings::docs_map_t& temp_docs = GlobalSettings::docs();
sprite::Map& temp_settings = GlobalSettings::map();
constexpr double video_chooser_column_width = 300;
}

sprite::Map& settings_map() {
    return settings_scene::temp_settings;
}

void Combobox::init() {
    _dropdown = std::make_shared<Dropdown>(Bounds(0, 0, settings_scene::video_chooser_column_width, 28));
    auto keys = settings_map().keys();
    _dropdown->set_items(std::vector<Dropdown::TextItem>(keys.begin(), keys.end()));
    _dropdown->on_select([this](long_t, const Dropdown::TextItem & item){
        auto name = item.name();
        if(settings_map().has(name)) {
            set(ParmName{name});
        }
        _dropdown->set_opened(false);
    });
    
    //set_background(_settings.fill_clr, _settings.line_clr);
    set_bounds(_settings.bounds);
    set_clickable(true);
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
}

void Combobox::set(ParmName name) {
    if(name != _settings.param) {
        _settings.param = name;
        _value = LabeledField::Make(name);
        if(not dynamic_cast<const LabeledCheckbox*>(_value.get())) {
            _value->set_description("");
        }
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

void Combobox::set_bounds(const Bounds& bds) {
    if(not _settings.bounds.Equals(bds)) {
        _settings.bounds = bds;
        _dropdown->set(attr::Size(_dropdown->width(), bds.height));
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
        _dropdown->set(attr::Size{_dropdown->width(), p.height});
        set_content_changed(true);
    }
    Entangled::set_size(p);
}

LabeledCombobox::LabeledCombobox(const std::string& name)
    : LabeledField(name),
    _combo(std::make_shared<Combobox>())
{}

void LabeledCombobox::update() {
    LabeledField::update();
}

void LabeledField::set_description(std::string desc) {
    _text->set_txt(desc);
}

std::unique_ptr<LabeledField> LabeledField::Make(std::string parm, std::string desc) {
    auto ptr = Make(parm);
    ptr->set_description(desc);
    return ptr;
}

std::unique_ptr<LabeledField> LabeledField::Make(std::string parm, bool invert) {
    if(parm.empty()) {
        auto ptr = std::make_unique<LabeledCombobox>();
        return ptr;
    }
    
    if(not settings_map().has(parm))
        throw U_EXCEPTION("GlobalSettings has no parameter named ", parm,".");
    auto ref = settings_map()[parm];
    
    std::unique_ptr<LabeledField> ptr;
    if(ref.is_type<bool>()) {
        ptr = std::make_unique<LabeledCheckbox>(parm, invert);
        
    } else if(ref.is_type<std::string>()) {
        ptr = std::make_unique<LabeledTextField>(parm);
        ptr->representative().to<Textfield>()->set_text(ref.value<std::string>());
    } else if(ref.is_type<int>() || ref.is_type<float>() || ref.is_type<double>()
              || ref.is_type<uint8_t>() || ref.is_type<uint16_t>()
              || ref.is_type<uint64_t>()
              || ref.is_type<timestamp_t>())
    {
        ptr = std::make_unique<LabeledTextField>(parm);
        ptr->representative().to<Textfield>()->set_text(ref.get().valueString());
    } else if(ref.is_type<file::Path>()) {
        ptr = std::make_unique<LabeledPath>(parm, ref.value<file::Path>());
        //ptr->representative().to<Dropdown>()->set_text(ref.value<file::Path>().str());
    } else if(ref.get().is_enum()) {
        ptr = std::make_unique<LabeledList>(parm);
        auto list = ptr->representative().to<ScrollableList<>>();
        auto values = ref.get().enum_values()();
        auto index = ref.get().enum_index()();
        
        list->set_items(values);
        list->select_item(index);
        
        //ptr = std::make_unique<LabeledDropDown>(parm);
        
        //ptr->representative().to<Dropdown>()->set_items(std::vector<Dropdown::TextItem>(values.begin(), values.end()));
        //ptr->representative().to<Dropdown>()->select_item(index);
    } else {
        ptr = std::make_unique<LabeledTextField>(parm);
        ptr->representative().to<Textfield>()->set_text(ref.get().valueString());
        //throw U_EXCEPTION("Cannot find the appropriate control for type ", ref.get().type_name());
    }
    return ptr;
}

LabeledCheckbox::LabeledCheckbox(const std::string& name, bool invert)
: LabeledField(name),
_checkbox(std::make_shared<gui::Checkbox>(attr::Loc(), name)),
_invert(invert),
_ref(settings_map()[name])
{
    _docs = settings_scene::temp_docs[name];
    
    _checkbox->set_checked(_invert
                           ? not _ref.value<bool>()
                           : _ref.value<bool>());
    print(name, ": Initially set checkbox to ", _checkbox->checked(), " with invert ", _invert, " for ", _ref.get().valueString());
    _checkbox->set_font(Font(0.7f));
    
    _checkbox->on_change([this](){
        try {
            print("Setting ", _ref.toStr(), " before = ", _ref.get().valueString());
            if(_invert)
                _ref.get() = not _checkbox->checked();
            else
                _ref.get() = _checkbox->checked();
            
            print("Setting setting ",_ref.toStr(), " = ",_ref.get().valueString(), " with ",_checkbox->checked()," and invert = ", _invert);
            
        } catch(...) {}
    });
}

void LabeledCheckbox::update() {
    _checkbox->set_checked(_invert ? not _ref.value<bool>() : _ref.value<bool>());
    print("Setting checkbox to ", _checkbox->checked());
}

void LabeledCheckbox::set_description(std::string desc) {
    _checkbox.to<Checkbox>()->set_text(desc);
}

LabeledTextField::LabeledTextField(const std::string& name)
: LabeledField(name),
_text_field(std::make_shared<gui::Textfield>(Bounds(0, 0, settings_scene::video_chooser_column_width, 28))),
_ref(settings_scene::temp_settings[name])
{
    _text_field->set_placeholder(name);
    _text_field->set_font(Font(0.7f));
    
    _docs = settings_scene::temp_docs[name];
    
    update();
    _text_field->on_text_changed([this](){
        try {
            _ref.get().set_value_from_string(_text_field->text());
            
        } catch(...) {}
    });
}

void LabeledTextField::update() {
    auto str = _ref.get().valueString();
    if(str.length() >= 2 && str.front() == '"' && str.back() == '"') {
        str = str.substr(1,str.length()-2);
    }
    _text_field->set_text(str);
}

LabeledList::LabeledList(const std::string& name)
: LabeledField(name),
_list(std::make_shared<gui::ScrollableList<>>(Bounds(0, 0, settings_scene::video_chooser_column_width, 28))),
_ref(settings_scene::temp_settings[name])
{
    _docs = settings_scene::temp_docs[name];
    
    //_list->textfield()->set_font(Font(0.7f));
    _list->set(Font(0.7f));
    assert(_ref.get().is_enum());
    std::vector<std::string> items;
    for(auto &name : _ref.get().enum_values()()) {
        items.push_back(name);
    }
    _list->set_items(items);
    _list->select_item(narrow_cast<long>(_ref.get().enum_index()()));
    
    _list->on_select([this](auto index, auto) {
        if(index < 0)
            return;
        
        try {
            _ref.get().set_value_from_string(_ref.get().enum_values()().at((size_t)index));
        } catch(...) {}
        
        //_dropdown->set_opened(false);
    });
}

void LabeledList::update() {
    _list->select_item(narrow_cast<long>(_ref.get().enum_index()()));
}

LabeledDropDown::LabeledDropDown(const std::string& name)
: LabeledField(name),
_dropdown(std::make_shared<gui::Dropdown>(Bounds(0, 0, settings_scene::video_chooser_column_width, 28))),
_ref(settings_scene::temp_settings[name])
{
    _docs = settings_scene::temp_docs[name];
    
    _dropdown->textfield()->set_font(Font(0.7f));
    assert(_ref.get().is_enum());
    std::vector<Dropdown::TextItem> items;
    int index = 0;
    for(auto &name : _ref.get().enum_values()()) {
        items.push_back(Dropdown::TextItem((std::string)file::Path(name).filename(), index++, name));
    }
    _dropdown->set_items(items);
    _dropdown->select_item(narrow_cast<long>(_ref.get().enum_index()()));
    _dropdown->textfield()->set_text(_ref.get().valueString());
    
    _dropdown->on_select([this](auto index, auto) {
        if(index < 0)
            return;
        
        try {
            _ref.get().set_value_from_string(_ref.get().enum_values()().at((size_t)index));
        } catch(...) {}
        
        _dropdown->set_opened(false);
    });
}

void LabeledDropDown::update() {
    _dropdown->select_item(narrow_cast<long>(_ref.get().enum_index()()));
}

LabeledPath::FileItem::FileItem(const file::Path& path) : _path(path)
{
    
}

LabeledPath::FileItem::operator std::string() const {
    return std::string(_path.filename());
}

Color LabeledPath::FileItem::base_color() const {
    return _path.is_folder() ? Color(80, 80, 80, 200) : Color(100, 100, 100, 200);
}

Color LabeledPath::FileItem::color() const {
    return _path.is_folder() ? Color(180, 255, 255, 255) : White;
}

LabeledPath::LabeledPath(std::string name, file::Path path)
    : LabeledField(),
    _ref(settings_scene::temp_settings[name]),
    _files([](const file::Path& A, const file::Path& B) -> bool {
        return (A.is_folder() && !B.is_folder()) || (A.is_folder() == B.is_folder() && A.str() < B.str()); //A.str() == ".." || (A.str() != ".." && ((A.is_folder() && !B.is_folder()) || (A.is_folder() == B.is_folder() && A.str() < B.str())));
    }), _path(path)
{
    _dropdown = std::make_shared<gui::Dropdown>(Bounds(0, 0, settings_scene::video_chooser_column_width, 28));
    
    _dropdown->on_select([this](long_t, const Dropdown::TextItem &item) {
        file::Path path = item.name();
        if(path.empty()) {
            path = _dropdown->textfield()->text();
        }
        
        print("Selected ", path, " item.name()=",item.name(), " display_name=", item.display_name());
        
        _ref.get() = path;
        
        if(!_validity || _validity(path))
        {
            //file_selected(0, path.str());
            if(path.is_folder()) {
                if(_path != path)
                    change_folder(path);
            }
                
            _dropdown->select_textfield();
        } else
            FormatError("Path ",path.str()," cannot be opened.");
    });
    
    _dropdown->on_text_changed([this](std::string str) {
        auto path = file::Path(str);
        auto file = (std::string)path.filename();
        
        _ref.get() = path;
        
        if(path.empty() || (path == _path || ((!path.exists() || !path.is_folder()) && path.remove_filename() == _path)))
        {
            // still in the same folder
        } else if(utils::endsWith(str, file::Path::os_sep()) && path != _path && path.is_folder())
        {
            change_folder(path);
        } else if(not path.empty() && not path.remove_filename().empty() && path.remove_filename().is_folder() && _path != path.remove_filename()) {
            change_folder(path.remove_filename());
        }
    });
    
    _dropdown->textfield()->set_text(path.str());
    change_folder(path);
}

void LabeledPath::change_folder(const file::Path& p) {
    auto org = _path;
    auto copy = _files;
    
    /*if(p.str() == "..") {
        try {
            _path = _path.remove_filename();
            auto files = _path.find_files("");
            _files.clear();
            _files.insert(files.begin(), files.end());
            _files.insert("..");
            
            //_list->set_scroll_offset(Vec2());
            if(not utils::beginsWith(_dropdown->textfield()->text(), _path.str()))
                _dropdown->textfield()->set_text(_path.str());
            
        } catch(const UtilsException&) {
            _path = org;
            _files = copy;
        }
        update_names();
        
    } else*/
    if(p.is_folder()) {
        try {
            try {
                if(not p.empty())
                    _path = p.absolute();
                else
                    _path = p;
            } catch (...) {
                _path = p;
            }
            
            auto files = _path.find_files("");
            _files.clear();
            _files.insert(files.begin(), files.end());
            //_files.insert("..");
            
            //_list->set_scroll_offset(Vec2());
            //if(not utils::beginsWith(_dropdown->textfield()->text(), _path.str()))
            if(not _path.empty()) {
                _dropdown->textfield()->set_text(_path.str()+file::Path::os_sep());
            }
            
        } catch(const UtilsException&) {
            _path = org;
            _files = copy;
        }
        update_names();
    }
}

void LabeledPath::update_names() {
    _names.clear();
    _search_items.clear();
    for(auto &f : _files) {
        if(f.str() == ".." || !utils::beginsWith((std::string)f.filename(), '.')) {
            _names.push_back(FileItem(f));
            _search_items.push_back(Dropdown::TextItem(f.str()));
            _search_items.back().set_display(std::string(file::Path(f.str()).filename()));
            print("* adding ", _search_items.back().name(), " | ", _search_items.back().display_name());
        }
    }
    //_list->set_items(_names);
    _dropdown->set_items(_search_items);
}

void LabeledPath::update() {
    
}

}
