#include "LabeledField.h"
#include <misc/GlobalSettings.h>
#include <gui/ParseLayoutTypes.h>

namespace gui::dyn {

namespace settings_scene {
GlobalSettings::docs_map_t& temp_docs = GlobalSettings::docs();
sprite::Map& temp_settings = GlobalSettings::map();
constexpr double video_chooser_column_width = 300;
}

sprite::Map& settings_map() {
    return settings_scene::temp_settings;
}

LabeledField::LabeledField(const std::string& name)
    : _text(std::make_shared<gui::Text>(name)), _ref(settings_map()[name])
{
    //if(name.empty())
    //    throw std::invalid_argument("Name cannot be empty.");
    _text->set_font(Font(0.6f));
    _text->set_color(White);
    
    if(_ref.get().valid()) {
        print("Registering callback for ", name);
        
        settings_map().register_callback(Meta::toStr((uint64_t)this), [this, name](auto, auto&, auto& n, auto&)
                                         {
            if(n == name) {
                update();
            }
        });
    } else {
        print("Ref invalid.");
    }
}

LabeledField::~LabeledField() {
    if(_ref.get().valid())
        settings_map().unregister_callback(Meta::toStr((uint64_t)this));
}

void ErrorElement::init() {
    _text = std::make_shared<Text>("ERROR: element "+name()+" cannot be loaded.");
    set_bounds(_settings.bounds);
}

void ErrorElement::update() {
    if(not content_changed())
        return;
    
    set_background(_settings.fill_clr, _settings.line_clr);
    _text->set_txt(_settings.content);
    _text->set_font(_settings.font);
    //auto_size({});
    
    begin();
    advance_wrap(*_text);
    end();
}

void ErrorElement::set(attr::Content content) {
    if(_settings.content != content) {
        _settings.content = content;
        set_content_changed(true);
    }
}
void ErrorElement::set(attr::FillClr clr) {
    if(_settings.fill_clr != clr) {
        _settings.fill_clr = clr;
        Entangled::set(clr);
        set_content_changed(true);
    }
}
void ErrorElement::set(attr::LineClr clr) {
    if(_settings.line_clr != clr) {
        _settings.line_clr = clr;
        Entangled::set(clr);
        set_content_changed(true);
    }
}
void ErrorElement::set(attr::TextClr clr) {
    if(_settings.text_clr != clr) {
        _settings.text_clr = clr;
        if(_text)
            _text->set(clr);
        set_content_changed(true);
    }
}

void ErrorElement::set_bounds(const Bounds& bds) {
    if(not _settings.bounds.Equals(bds)) {
        _settings.bounds = bds;
        set_content_changed(true);
    }
    Entangled::set_bounds(bds);
}

void ErrorElement::set_pos(const Vec2& p) {
    if(not _settings.bounds.pos().Equals(p)) {
        _settings.bounds << p;
        set_content_changed(true);
    }
    Entangled::set_pos(p);
}

void ErrorElement::set_size(const Size2& p) {
    if(not _settings.bounds.size().Equals(p)) {
        _settings.bounds << p;
        set_content_changed(true);
    }
    Entangled::set_size(p);
}

LabeledCombobox::LabeledCombobox(const std::string& name, const nlohmann::json& obj)
    : LabeledField(name),
    _combo(std::make_shared<Combobox>())
{}

void LabeledCombobox::update() {
    LabeledField::update();
}

void LabeledField::set_description(std::string desc) {
    _text->set_txt(desc);
}

std::unique_ptr<LabeledField> LabeledField::Make(std::string parm, const nlohmann::json& obj, bool invert) {
    if(parm.empty()) {
        auto ptr = std::make_unique<LabeledCombobox>("", obj);
        return ptr;
    }
    
    if(not settings_map().has(parm))
        throw U_EXCEPTION("GlobalSettings has no parameter named ", parm,".");
    auto ref = settings_map()[parm];
    
    std::unique_ptr<LabeledField> ptr;
    /*if(ref.is_type<bool>()) {
        ptr = std::make_unique<LabeledCheckbox>(parm, parm, obj,invert);
        
    } else*/ if(ref.is_type<std::string>()) {
        ptr = std::make_unique<LabeledTextField>(parm, obj);
        ptr->representative().to<Textfield>()->set_text(ref.value<std::string>());
    } else if(ref.is_type<int>() || ref.is_type<float>() || ref.is_type<double>()
              || ref.is_type<uint8_t>() || ref.is_type<uint16_t>()
              || ref.is_type<uint64_t>()
              || ref.is_type<timestamp_t>())
    {
        ptr = std::make_unique<LabeledTextField>(parm, obj);
        ptr->representative().to<Textfield>()->set_text(ref.get().valueString());
    } else if(ref.is_type<file::Path>()) {
        ptr = std::make_unique<LabeledPath>(parm, parm, ref.value<file::Path>());
        //ptr->representative().to<Dropdown>()->set_text(ref.value<file::Path>().str());
    } else if(ref.is_type<file::PathArray>()) {
        ptr = std::make_unique<LabeledPathArray>(parm, obj);
        
    } else if(ref.is_type<bool>() || ref.get().is_enum()) {
        ptr = std::make_unique<LabeledList>(parm, obj);
        auto list = ptr->representative().to<List>();
        
        /*std::vector<std::shared_ptr<List::Item>> vs;
        size_t index{0};
        if(ref.is_type<bool>()) {
            vs = {
                std::make_shared<TextItem>("false", 0),
                std::make_shared<TextItem>("true", 1)
            };
            index = ref.get().value<bool>() ? 1 : 0;
            
        } else {
            auto values = ref.get().enum_values()();
            index = ref.get().enum_index()();
            
            for(auto& v : values)
                vs.push_back(std::make_shared<TextItem>(v));
        }*/
        
        
        //ptr = std::make_unique<LabeledDropDown>(parm);
        
        //ptr->representative().to<Dropdown>()->set_items(std::vector<Dropdown::TextItem>(values.begin(), values.end()));
        //ptr->representative().to<Dropdown>()->select_item(index);
    } else {
        ptr = std::make_unique<LabeledTextField>(parm, obj);
        ptr->representative().to<Textfield>()->set_text(ref.get().valueString());
        //throw U_EXCEPTION("Cannot find the appropriate control for type ", ref.get().type_name());
    }
    return ptr;
}

LabeledCheckbox::LabeledCheckbox(const std::string& name, const std::string& desc, const nlohmann::json&, bool invert)
: LabeledField(name),
_checkbox(std::make_shared<gui::Checkbox>(attr::Loc(), desc)),
_invert(invert)
{
    _docs = settings_scene::temp_docs[name];
    
    _checkbox->set_checked(_invert
                           ? not _ref.value<bool>()
                           : _ref.value<bool>());
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
    print("Checkbox for ", _ref.get().name(), " is ", _checkbox->checked());
}

void LabeledCheckbox::set_description(std::string desc) {
    _checkbox.to<Checkbox>()->set_text(desc);
}

LabeledCheckbox::~LabeledCheckbox() {
}

LabeledTextField::LabeledTextField(const std::string& name, const nlohmann::json& obj)
: LabeledField(name),
_text_field(std::make_shared<gui::Textfield>(Bounds(0, 0, settings_scene::video_chooser_column_width, 28)))
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
    if(str != _text_field.to<Textfield>()->text()) {
        _text_field->set_text(str);
        print("Updating textfield for ", _ref.get().name(), " -> ", str);
    }
}

LabeledList::LabeledList(const std::string& name, const nlohmann::json& obj)
: LabeledField(name),
_list(std::make_shared<gui::List>(
    Bounds(0, 0, settings_scene::video_chooser_column_width, 28),
  std::string(), std::vector<std::shared_ptr<List::Item>>{}, [this](List* list, const List::Item& item){
      auto index = item.ID();
      if(index < 0)
          return;
      
      try {
          if(_ref.is_type<bool>()) {
              _ref = index == 1 ? true : false;
              
          } else {
              assert(_ref.get().is_enum());
              auto name = _ref.get().enum_values()().at((size_t)index);
              auto current = _ref.get().valueString();
              if(name != current)
                  _ref.get().set_value_from_string(name);
          }
          
      } catch(...) {}
      
      list->set_folded(true);
  }))
{
    _docs = settings_scene::temp_docs[name];
    
    //_list->textfield()->set_font(Font(0.7f));
    //_list->set(Font(0.7f));
    std::vector<std::shared_ptr<List::Item>> items;
    long index{0};
    if(_ref.is_type<bool>()) {
        items = {
            std::make_shared<TextItem>("false", 0),
            std::make_shared<TextItem>("true", 1)
        };
        index = _ref.value<bool>() ? 1 : 0;
        
    } else {
        assert(_ref.get().is_enum());
        
        for(auto &name : _ref.get().enum_values()()) {
            items.push_back(std::make_shared<TextItem>(name));
        }
        index = narrow_cast<long>(_ref.get().enum_index()());
    }
    _list->set_items(items);
    _list->select_item(index);
    /*_list->on_select([this](auto index, auto) {
        if(index < 0)
            return;
        
        try {
            auto name = _ref.get().enum_values()().at((size_t)index);
            auto current = _ref.get().valueString();
            if(name != current)
                _ref.get().set_value_from_string(name);
        } catch(...) {}
        
        //_dropdown->set_opened(false);
    });*/
    
    _list->set_foldable(true);
    _list->set_folded(true);
    //list->set_items(vs);
    //list->select_item(index);
    _list->add_event_handler(EventType::SELECT, [list = _list.get()](Event e) {
        if(not e.select.selected) {
            if(list->stage()->selected_object()
               && list->stage()->selected_object()->is_child_of(list))
            {
                return;
            }
            list->set_folded(true);
        }
    });
}

void LabeledList::update() {
    if(_ref.is_type<bool>()) {
        _list->select_item(_ref.value<bool>() ? 1 : 0);
    } else {
        _list->select_item(narrow_cast<long>(_ref.get().enum_index()()));
    }
    
}

LabeledDropDown::LabeledDropDown(const std::string& name, const nlohmann::json& obj)
: LabeledField(name),
_dropdown(std::make_shared<gui::Dropdown>(Bounds(0, 0, settings_scene::video_chooser_column_width, 28)))
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

LabeledPath::LabeledPath(std::string name, const std::string& desc, file::Path path)
    : LabeledField(name),
    _files([](const file::Path& A, const file::Path& B) -> bool {
        return (A.is_folder() && !B.is_folder()) || (A.is_folder() == B.is_folder() && A.str() < B.str()); //A.str() == ".." || (A.str() != ".." && ((A.is_folder() && !B.is_folder()) || (A.is_folder() == B.is_folder() && A.str() < B.str())));
    }), _path(path)
{
    set_description("");
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
        
        if(_ref.get().value<file::Path>() == path)
            return;
        
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
    if(_ref.value<file::Path>() != file::Path(_dropdown->textfield()->text())) {
        print("Value of ", _ref.get().name(), " changed -> ", _ref.value<file::Path>(), " vs. ", _dropdown->textfield()->text());
        _dropdown->textfield()->set_text(_ref.value<file::Path>().str());
    }
}

LabeledPathArray::LabeledPathArray(const std::string& name, const nlohmann::json& obj)
    : LabeledField(name)
{
    // Initialize Dropdown, attach handlers for events
    _dropdown = Layout::Make<gui::Dropdown>(/*args*/);
    _dropdown->on_text_changed([this](std::string){
        // handle text changed
        updateDropdownItems();
        updateStaticText(); // Function to update the preview based on the new pattern
    });

    // Initialize StaticText
    _staticText = Layout::Make<gui::StaticText>();
    _layout = Layout::Make<gui::VerticalLayout>(std::vector<Layout::Ptr>{_dropdown, _staticText});
    _layout->set_margins(Bounds());
    _layout->set_policy(VerticalLayout::Policy::LEFT);
    
    if(obj.contains("preview") && obj["preview"].is_object()) {
        auto &text = obj["preview"];
        if(text.contains("font")) {
            Font font = parse_font(text);
            _staticText->set(font);
        }
        if(text.contains("color")) {
            try {
                auto color = parse_color(text["color"]);
                _staticText->set(TextClr{color});
            } catch(const std::exception& ex) {
                FormatExcept("Invalid format for color: ", ex.what());
            }
        }
    }
}

void LabeledPathArray::updateStaticText() {
    std::string pattern = _dropdown->textfield()->text();
    // perform pattern matching using PathArray
    auto matches = file::PathArray(pattern).get_paths();
    if (matches.empty()) {
        _staticText->set_txt("No matches found");
    } else {
        _staticText->set_txt(Meta::toStr(matches));
    }
}

void LabeledPathArray::add_to(std::vector<Layout::Ptr>& v) {
    assert(_dropdown != nullptr);
    assert(_staticText != nullptr);
    assert(_layout != nullptr);
    v.push_back(_layout);
    //v.push_back(_dropdown);
    //v.push_back(_staticText);
}

void LabeledPathArray::updateDropdownItems() {
    try {
        // Create a PathArray object using the pattern from the dropdown textfield
        file::PathArray pathArray(_dropdown->textfield()->text());

        // Populate _files based on pattern
        auto matches = pathArray.get_paths();
        _files.clear();
        _files.insert(matches.begin(), matches.end());

        // Populate _search_items
        _search_items.clear();
        for(auto &f : _files) {
            if(f.str() != ".." && !utils::beginsWith((std::string)f.filename(), '.')) {
                _search_items.push_back(Dropdown::TextItem(f.str()));
                _search_items.back().set_display(std::string(file::Path(f.str()).filename()));
            }
        }
        
        // Update dropdown items
        _dropdown->set_items(_search_items);

    } catch(const UtilsException&) {
        // Handle exception
        // Maybe reset _files to some safe state
    }
}

void LabeledPathArray::update() {
    // Refresh dropdown items
    // ... other update logic
}

}
