#include "LabeledField.h"
#include <misc/GlobalSettings.h>
#include <gui/ParseLayoutTypes.h>
#include <gui/types/ErrorElement.h>
#include <misc/default_settings.h>

namespace gui {
class PathArrayView : public VerticalLayout {
    using VerticalLayout::VerticalLayout;
public:
    StaticText* text{nullptr};
    Drawable* control{nullptr};
    void update_layout() override {
        //set(LineClr{Red});
        //print("Updating layout.");
        if(text && control) {
            //text->set(SizeLimit{control->width(), control->height()});
        }
        VerticalLayout::update_layout();
    }
};
}

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
    : _text(std::make_shared<gui::Text>(Str{name})), _ref(settings_map()[name])
{
    //if(name.empty())
    //    throw std::invalid_argument("Name cannot be empty.");
    _text->set_font(Font(0.6f));
    _text->set_color(White);
    
    if(_ref.get().valid()) {
        print("Registering callback for ", name);
        _callback_id = settings_map().register_callbacks({name}, [this](auto){update();});
    }
#ifndef NDEBUG
    else {
        print("Ref invalid: ", name);
    }
#endif
}

LabeledField::~LabeledField() {
    if(_callback_id)
        settings_map().unregister_callbacks(std::move(_callback_id));
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
        
    } else if(ref.is_type<file::PathArray>()) {
        ptr = std::make_unique<LabeledPathArray>(parm, obj);
        
    } else if(ref.is_type<bool>() || ref.get().is_enum()) {
        ptr = std::make_unique<LabeledList>(parm, obj, invert);
        
    } else {
        ptr = std::make_unique<LabeledTextField>(parm, obj);
        ptr->representative().to<Textfield>()->set_text(ref.get().valueString());
        //throw U_EXCEPTION("Cannot find the appropriate control for type ", ref.get().type_name());
    }
    return ptr;
}

LabeledCheckbox::LabeledCheckbox(const std::string& name, const std::string& desc, const nlohmann::json&, bool invert)
: LabeledField(name),
_checkbox(std::make_shared<gui::Checkbox>(attr::Str(desc))),
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

LabeledTextField::LabeledTextField(const std::string& name, const nlohmann::json&)
    : LabeledField(name),
    _text_field(std::make_shared<gui::Textfield>(Box(0, 0, settings_scene::video_chooser_column_width, 28)))
{
    _text_field->set_placeholder(name);
    _text_field->set_font(Font(0.7f));
    
    _docs = settings_scene::temp_docs[name];
    
    update();
    _text_field->on_text_changed([this](){
        try {
            _ref.get().set_value_from_string(_text_field->text());
            
        } catch(...) {
            FormatExcept("Cannot convert ", _text_field->text(), " to ", _ref.get().type_name());
        }
    });
}

void LabeledTextField::update() {
    if(_text_field->selected()) {
        // dont update because of self-changes
        return;
    }
    std::string str;
    if(_ref.is_type<std::string>()) {
        str = _ref.value<std::string>();
    } else {
        str = _ref.get().valueString();
    }
    /*if(str.length() >= 2 && str.front() == '"' && str.back() == '"') {
        str = str.substr(1,str.length()-2);
    }*/
    if(str != _text_field.to<Textfield>()->text()) {
        _text_field->set_text(str);
    }
}

LabeledList::LabeledList(const std::string& name, const nlohmann::json& obj, bool invert)
: LabeledField(name),
_list(std::make_shared<gui::List>(
    Bounds(0, 0, settings_scene::video_chooser_column_width, 28),
  std::string(), std::vector<std::shared_ptr<List::Item>>{}, [this](List* list, const List::Item& item){
      auto index = item.ID();
      if(index < 0)
          return;
      
      try {
          if(_ref.is_type<bool>()) {
              if(_invert) {
                  _ref = index == 0 ? true : false;
              } else
                  _ref = index == 1 ? true : false;
              
          } else {
              assert(not _invert); // incompatible with enums
              assert(_ref.get().is_enum());
              auto name = _ref.get().enum_values()().at((size_t)index);
              auto current = _ref.get().valueString();
              if(name != current)
                  _ref.get().set_value_from_string(name);
          }
          
      } catch(...) {}
      
      list->set_folded(true);
  })),
    _invert(invert)
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
        index = not _invert ?
                    (_ref.value<bool>() ? 1 : 0)
                :   (_ref.value<bool>() ? 0 : 1);
        
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
        _list->select_item(not _invert ?
                                (_ref.value<bool>() ? 1 : 0)
                               :(_ref.value<bool>() ? 0 : 1));
    } else {
        _list->select_item(narrow_cast<long>(_ref.get().enum_index()()));
    }
    
}

LabeledDropDown::LabeledDropDown(const std::string& name, const nlohmann::json& obj)
: LabeledField(name),
_dropdown(std::make_shared<gui::Dropdown>(Box(0, 0, settings_scene::video_chooser_column_width, 28)))
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
    _dropdown->select_item(Dropdown::RawIndex{narrow_cast<long>(_ref.get().enum_index()())});
    _dropdown->textfield()->set_text(_ref.get().valueString());
    
    _dropdown->on_select([this](auto index, auto) {
        if(not index.valid())
            return;
        
        try {
            _ref.get().set_value_from_string(_ref.get().enum_values()().at((size_t)index.value));
        } catch(...) {}
        
        _dropdown->set_opened(false);
    });
}

void LabeledDropDown::update() {
    _dropdown->select_item(Dropdown::RawIndex{narrow_cast<long>(_ref.get().enum_index()())});
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
    _dropdown = std::make_shared<CustomDropdown>(Box(0, 0, 500, 28));
    _dropdown.to<CustomDropdown>()->textfield()->set_placeholder("Please enter a path...");
    
    _dropdown->on_select([this](auto, const Dropdown::TextItem &item) {
        file::Path path = item.name();
        if(path.empty())
            path = _dropdown->textfield()->text();
        
        if(!_validity || _validity(path))
        {
            print("Selected ", path, " item.name()=",item.name(), " display_name=", item.display_name());
            
            _ref.get() = path;
            
            if(path.is_folder()) {
                if(_path != path) {
                    change_folder(path);
                    _path = path;
                }
                
                _dropdown->select_textfield();
                _dropdown->set_opened(true);
            } else {
                _dropdown->set_opened(false);
            }
            //if(_dropdown->stage())
            //    _dropdown->stage()->select(nullptr);
            
        } else
            FormatError("Path ",path," cannot be opened.");
    });
    
    _dropdown->on_text_changed([this](std::string str) {
        auto path = file::Path(str);
        auto file = (std::string)path.filename();
        
        if(utils::endsWith(str, file::Path::os_sep()))
        {
            if(path.is_folder())
            {
                change_folder(path);
            }
            
        } else if(not path.empty()
                  && not path.remove_filename().empty()
                  && path.remove_filename().is_folder())
        {
            change_folder(path.remove_filename());
        }
        
        _path = path;
        if(_ref.get().value<file::Path>() != path)
            _ref.get() = path;
    });
    
    _dropdown->set(Textfield::OnEnter_t{
        [list = _dropdown->list().get()](){
            if(!list->items().empty()) {
                list->select_highlighted_item();
            }
        }
    });
    
    _dropdown->set_update_callback([this]() { asyncUpdateItems(); });
    _dropdown->list()->set(Placeholder_t("Loading..."));
    
    _path = path;
    _dropdown->textfield()->set_text(_path.str());
    _dropdown->auto_size({0,0});
    print("Dropdown -> ", _dropdown->bounds());
    
    asyncRetrieve([path](){
        if(path.is_folder())
            return path;
        else if(path.remove_filename().is_folder())
            return path.remove_filename();
        
        throw std::invalid_argument("Do nothing.");
    });
}

void LabeledPath::asyncUpdateItems() {
    tl::expected<std::vector<Dropdown::TextItem>, const char*> items;
    if (_file_retrieval.valid()
        && _file_retrieval.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready)
    {
        items = _file_retrieval.get();
    }
    else
        return;

    if (_should_reload.has_value()) {
        auto val = _should_reload.value();
        _should_reload.reset();
        asyncRetrieve(val);
        return;
    }
    
    if(not items) {
        //FormatWarning("Items did not retrieve successfully: ", items.error());
        return;
    }
    
    _dropdown->set_items(std::move(items.value()));
}

void LabeledPath::asyncRetrieve(std::function<file::Path()> fn) {
    if(_file_retrieval.valid()) {
        if(_file_retrieval.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready)
        {
            _should_reload = fn;
            return;
        }
        
        (void)_file_retrieval.get(); // discard
    }
    
    _file_retrieval = std::async(std::launch::async, [this, copy = _files](std::function<file::Path()> fn) -> tl::expected<std::vector<Dropdown::TextItem>, const char*> {
        try {
            //std::this_thread::sleep_for(std::chrono::seconds(1));
            auto p = fn();
            if(p.is_folder()) {
                try {
                    try {
                        if(not p.empty())
                            p = p.absolute();
                    } catch (...) { }
                    
                    auto files = p.find_files("");
                    auto items = updated_names(files);
                    
                    {
                        std::unique_lock guard(_file_mutex);
                        _files.clear();
                        _files.insert(files.begin(), files.end());
                    }
                    
                    if(_dropdown->stage()) {
                        std::unique_lock guard(_dropdown->stage()->lock());
                        _dropdown->set_content_changed(true);
                        _dropdown->set_dirty();
                    }
                    
                    return items;
                    
                } catch(const UtilsException& ex) {
                    //_files = copy;
                    // do nothing...
                    FormatWarning("Cannot retrieve files for ", p, ": ", ex.what());
                    return tl::unexpected(ex.what());
                }
            }
        } catch(...) { }
        
        return tl::unexpected("");
    }, fn);
    
    if(_file_retrieval.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready) 
    {
        asyncUpdateItems();
    } else {
        _dropdown->set_items({});
    }
}

void LabeledPath::change_folder(file::Path p) {
    if(_folder.has_value() && _folder.value() == p)
        return;
    
    _folder = p;
    asyncRetrieve([p]() { return p; });
}

std::vector<Dropdown::TextItem> LabeledPath::updated_names(const std::set<file::Path>& path) {
    std::vector<Dropdown::TextItem> search_items;
    //_names.clear();
    //_search_items.clear();
    for(auto &f : path) {
        if(f.str() == ".." || !utils::beginsWith((std::string)f.filename(), '.')) {
            //_names.push_back(FileItem(f));
            search_items.push_back(Dropdown::TextItem(f.str()));
            search_items.back().set_display(std::string(file::Path(f.str()).filename()));
            //print("* adding ", _search_items.back().name(), " | ", _search_items.back().display_name());
        }
    }
    //_list->set_items(_names);
    //_dropdown->set_items(_search_items);
    return search_items;
}

void LabeledPath::update() {
    if(_dropdown->textfield()->selected())
        return;
    
    if(_ref.value<file::Path>() != file::Path(_dropdown->textfield()->text())) {
        //print("Value of ", _ref.get().name(), " changed -> ", _ref.value<file::Path>(), " vs. ", _dropdown->textfield()->text());
        _dropdown->textfield()->set_text(_ref.value<file::Path>().str());
    }
}

LabeledPathArray::LabeledPathArray(const std::string& name, const nlohmann::json& obj)
    : LabeledField(name)
{
    // Initialize Dropdown, attach handlers for events
    _dropdown = Layout::Make<CustomDropdown>(attr::Size(300,40));
    _dropdown.to<CustomDropdown>()->textfield()->set_placeholder("Please enter a path or matching pattern...");
    _dropdown->on_text_changed([this](std::string str){
        // handle text changed
        updateDropdownItems();
    });
    _dropdown->set(ClosesAfterSelect(false));
    _dropdown->set([this](auto, const Dropdown::TextItem& item) {
        // on select
        _ref.container().set_do_print(true);
        print("Selecting ", item.name());
        auto path = file::Path(_dropdown->text());
        if(path.is_folder()) {
            _dropdown->textfield()->set_text(_dropdown->text()+file::Path::os_sep());
            _dropdown->select_textfield();
        } else {
            _dropdown->stage()->select(nullptr);
            _ref.value<file::PathArray>() = file::PathArray(_dropdown->text());
        }
        updateDropdownItems();

    });
    _dropdown->set(Textfield::OnEnter_t{
        [this, dropdown = _dropdown.to<Dropdown>()](){
            auto list = dropdown->list().get();
            auto text = _dropdown->text();
            
            if(_pathArray.matched_patterns()) {
                if(dropdown->stage())
                    dropdown->stage()->select(NULL);
                _ref.value<file::PathArray>() = _pathArray;
                
            } else if(not list->items().empty()) {
                list->select_highlighted_item();
            } else if(dropdown->on_select())
                dropdown->on_select()(Dropdown::RawIndex{}, Dropdown::TextItem::invalid_item());

            updateDropdownItems();
            list->set_last_hovered_item(-1);
        }
    });
    
    _dropdown->textfield()->set(Textfield::OnTab_t{
        [this](){
            if(not _dropdown->items().empty()) {
                auto dropdown = _dropdown.to<Dropdown>();
                //auto index = dropdown->hovered_item().index();
                //print("_selected_item ", dropdown->selected_item().index()," / hovered_item = ", index);
                //if(index != Dropdown::Item::INVALID_ID) {
                //    dropdown->select_item(index);
                //} else
                Dropdown::FilteredIndex index{0};
                if(index){
                    dropdown->select_item(dropdown->filtered_item_index(index));
                }
                
                auto path = dropdown->selected_item().name();
                print("selected ", dropdown->selected_item(), " at ", index, " => path=", path);
                if(file::Path(path).is_folder() && not utils::endsWith(path, file::Path::os_sep()))
                {
                    _dropdown->textfield()->set_text(path+file::Path::os_sep());
                } else {
                    _dropdown->textfield()->set_text(path);
                }
                
                updateDropdownItems();
            }
        }
    });
    
    // Initialize StaticText
    _staticText = Layout::Make<gui::StaticText>(StaticText::FadeOut_t{1.f});
    
    _layout = Layout::Make<gui::PathArrayView>(std::vector<Layout::Ptr>{_dropdown, _staticText}, attr::Margins{0, 0, 0, 0});
    auto view = static_cast<PathArrayView*>(_layout.get());
    view->text = (StaticText*)_staticText.get();
    view->control = _dropdown.get();
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
        if(text.contains("max_size")) {
            try {
                auto max_size = Meta::fromStr<Size2>(text["max_size"].dump());
                _staticText->set(SizeLimit{max_size});
            } catch(const std::exception& ex) {
                FormatExcept("Invalid format for max_size: ", ex.what());
            }
        }
    }
    
    _dropdown->set_update_callback([this]() { asyncUpdateItems(); });
    _dropdown->textfield()->set(Str{ _ref.value<file::PathArray>().source() });
    updateDropdownItems();
}

LabeledPathArray::~LabeledPathArray() {
    if (_future.valid())
        _future.get(); // finalize future
}

void CustomDropdown::update() {
    if(_update_callback)
        _update_callback();
    Dropdown::update();
}

void LabeledPathArray::updateStaticText(const std::vector<file::Path>& matches) {
    std::string pattern = _dropdown->textfield()->text();
    // perform pattern matching using PathArray
    if (matches.empty()) {
        _staticText->set_txt("No matches found");
    } else {
        _staticText->set_txt(settings::htmlify( Meta::toStr(matches) ));
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

void LabeledPathArray::asyncUpdateItems() {
    if (_future.valid()
        && _future.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready)
    {
        _tmp_files = _future.get();
    }
    else
        return;

    if (_should_update) {
        _should_update = false;
        updateDropdownItems();
        return;
    }

    _pathArray = std::move(_pathArrayCopy);
    //print("Setting value = ", _pathArray);
    _ref.value<file::PathArray>() = _pathArray;

    try {
        // Always update _search_items based on _files
        _search_items.clear();
        for (const auto& f : _tmp_files) {
            if (f.str() != ".." && !utils::beginsWith((std::string)f.filename(), '.')) {
                Dropdown::TextItem p(f.str());
                p.set_display(std::string(file::Path(f.str()).filename()));
                p.set_color(f.is_folder() ? Color(255, 0, 0, 255) : Transparent);
                _search_items.emplace_back(std::move(p));
            }
        }

        _dropdown->set_items(_search_items);

    }
    catch (const UtilsException&) {
        // Handle exception
        // Maybe reset _files to some safe state
    }

    updateStaticText(_tmp_files);
    _tmp_files.clear();
}

void LabeledPathArray::updateDropdownItems() {
    if (_pathArray.source() == _dropdown->text())
        return;

    if(_future.valid())
        _should_update = true;
    else {
        _future = std::async(std::launch::async, [this, text = _dropdown->text(), ptr = _dropdown.get(), files = std::move(_tmp_files)]() {
            std::vector<file::Path> matches; // Temporary storage for matched or listed files
            
            try {
                file::PathArray pathArray(text);

                if ((file::Path(pathArray.source()).is_folder() && file::Path(_pathArrayCopy.source()) == file::Path(pathArray.source()))
                    || (not file::Path(pathArray.source()).is_folder()
                        && not file::Path(_pathArrayCopy.source()).is_folder()
                        && file::Path(pathArray.source()).remove_filename() == file::Path(_pathArrayCopy.source()).remove_filename()))
                {
                    print(_pathArrayCopy.source(), " is essentially the same as ", pathArray.source(), ", using our results here for ", _pathArrayCopy.source(), ".");
                    _pathArrayCopy.set_source(pathArray.source());
                    return files;
                }

                if (utils::endsWith(text, file::Path::os_sep())) {
                    // User is navigating into a folder
                    matches = pathArray.get_paths();
                    if (not pathArray.matched_patterns()) {
                        if (matches.empty() || matches.front().is_folder()) {
                            auto folder = matches.empty() ? file::Path("/") : matches.front();
                            auto files = folder.find_files();
                            matches = { files.begin(), files.end() };
                        }
                    }

                }
                else {
                    file::Path p(text);
                    // Handle wildcard matching or parent folder display
                    if (pathArray.matched_patterns()) {
                        // If wildcards are present, filter based on them
                        matches = pathArray.get_paths();
                    }
                    else if (p.exists() && p.is_regular())
                    {
                        matches = pathArray.get_paths();
                    }
                    else if (p.exists() && p.is_folder()) {
                        auto files = p.find_files();
                        matches = { files.begin(), files.end() };
                    }
                    else {
                        // If a separator was deleted, show parent folder contents
                        auto parentPath = p.remove_filename();
                        auto parentFiles = parentPath.find_files();
                        matches = { parentFiles.begin(), parentFiles.end() };
                        //matches = pathArray.get_paths();
                    }
                }

                //print("items for ", text, " = ", matches, " from ", pathArray);
                _pathArrayCopy = std::move(pathArray);
            }
            catch (...) {
                FormatExcept("Cannot parse ", text, " as PathArray.");
            }

            for (auto& m : matches)
                m.is_folder(); // cache is_folder

            if (ptr->stage()) {
                std::unique_lock guard(ptr->stage()->lock());
                ptr->set_content_changed(true);
                ptr->set_dirty();
                //print("Returning + setting dirty for ", text);
                return matches;
            }
            //print("Returning for ", text);
            return matches;
        });

        if (_future.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready) {
            asyncUpdateItems();
        }
    }
}

void LabeledPathArray::update() {
    // Refresh dropdown items
    // ... other update logic
    if (_dropdown->selected())
        return;

    if (_ref.value<file::PathArray>().source() != _dropdown->text()) {
        updateDropdownItems();
    }
}

}
