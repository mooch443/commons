#pragma once

#include <commons.pc.h>
#include <nlohmann/json.hpp>
#include <gui/colors.h>
#include <file/Path.h>
#include <gui/types/Layout.h>
#include <misc/PackLambda.h>
#include <misc/SpriteMap.h>
#include <misc/Timer.h>
#include <gui/GuiTypes.h>
#include <gui/types/Textfield.h>
#include <gui/types/Dropdown.h>
#include <gui/types/Checkbox.h>
#include <gui/types/List.h>
#include <regex>

namespace gui {
namespace dyn {

template<typename T, typename A>
concept takes_attribute = requires(T t, A a) {
    { t.set(a) } -> std::same_as<void>;
};

sprite::Map& settings_map();

struct LabeledField {
    gui::derived_ptr<gui::Text> _text;
    std::string _docs;
    //gui::derived_ptr<gui::HorizontalLayout> _joint;
    
    LabeledField(const std::string& name = "")
        : _text(std::make_shared<gui::Text>(name))
          //_joint(std::make_shared<gui::HorizontalLayout>(std::vector<Layout::Ptr>{_text, _text_field}))
    {
        _text->set_font(Font(0.6f));
        _text->set_color(White);
    }
    
    virtual ~LabeledField() {}
    
    virtual void add_to(std::vector<Layout::Ptr>& v) {
        v.push_back(_text);
    }
    virtual void update() {}
    virtual Layout::Ptr representative() { return _text; }
    virtual void set_description(std::string);
    
    template<typename T>
    void set(T attribute) {
        if constexpr(takes_attribute<Drawable, T>) {
            representative()->set(attribute);
            
        } else if(representative().is<Button>()) {
            if constexpr(takes_attribute<Button, T>)
            {
                representative().to<Button>()->set(attribute);
            }
            
        } else if(representative().is<Textfield>()) {
            if constexpr(takes_attribute<Textfield, T>)
            {
                representative().to<Textfield>()->set(attribute);
            }
            
        } else if(representative().is<Dropdown>()) {
            if constexpr(takes_attribute<Dropdown, T>)
            {
                representative().to<Dropdown>()->set(attribute);
            }
            
        } else if(representative().is<Checkbox>()) {
            if constexpr(takes_attribute<Checkbox, T>)
            {
                representative().to<Checkbox>()->set(attribute);
            }
        } else if(representative().is<List>()) {
            if constexpr(takes_attribute<List, T>)
            {
                representative().to<List>()->set(attribute);
            }
        } else if(representative().is<ScrollableList<>>()) {
            if constexpr(takes_attribute<ScrollableList<>, T>)
            {
                representative().to<ScrollableList<>>()->set(attribute);
            }
        }
    }
    
    static std::unique_ptr<LabeledField> Make(std::string parm);
    static std::unique_ptr<LabeledField> Make(std::string parm, std::string desc);
};

class Combobox : public Entangled {
public:
    struct Settings {
        Bounds bounds = Bounds(0, 0, 100, 33);
        Color fill_clr = Drawable::accent_color;
        Color line_clr = Black.alpha(200);
        Color text_clr = White;
        Font font = Font(0.75, Align::Center);
        std::string param;
    };
    
protected:
    Settings _settings;
    
    HorizontalLayout _layout;
    derived_ptr<Dropdown> _dropdown;
    std::unique_ptr<LabeledField> _value;
    
public:
    template<typename... Args>
    Combobox(Args... args)
    {
        create(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void create(Args... args) {
        (set(std::forward<Args>(args)), ...);
        init();
    }
    
public:
    using Entangled::set;
    
    void set(attr::Font font);
    void set(attr::FillClr clr) override;
    void set(attr::LineClr clr) override;
    void set(attr::TextClr clr);
    
    void set(std::function<void()> on_click) {
        if(on_click)
            this->on_click([on_click](auto) { on_click(); });
    }
    void set(ParmName name);
    
    void set_bounds(const Bounds& bds) override;
    void set_pos(const Vec2& p) override;
    void set_size(const Size2& p) override;
    
protected:
    void init();
    void update() override;
};

struct LabeledCombobox : public LabeledField {
    gui::derived_ptr<Combobox> _combo;
    LabeledCombobox(const std::string& name = "");
    void add_to(std::vector<Layout::Ptr>& v) override {
        LabeledField::add_to(v);
        v.push_back(_combo);
    }
    void update() override;
    Layout::Ptr representative() override { return _combo; }
};
struct LabeledTextField : public LabeledField {
    gui::derived_ptr<gui::Textfield> _text_field;
    sprite::Reference _ref;
    LabeledTextField(const std::string& name = "");
    void add_to(std::vector<Layout::Ptr>& v) override {
        LabeledField::add_to(v);
        v.push_back(_text_field);
    }
    void update() override;
    Layout::Ptr representative() override { return _text_field; }
};
struct LabeledDropDown : public LabeledField {
    gui::derived_ptr<gui::Dropdown> _dropdown;
    sprite::Reference _ref;
    LabeledDropDown(const std::string& name = "");
    void add_to(std::vector<Layout::Ptr>& v) override {
        LabeledField::add_to(v);
        v.push_back(_dropdown);
    }
    void update() override;
    Layout::Ptr representative() override { return _dropdown; }
};
struct LabeledList : public LabeledField {
    gui::derived_ptr<gui::ScrollableList<>> _list;
    sprite::Reference _ref;
    LabeledList(const std::string& name = "");
    void add_to(std::vector<Layout::Ptr>& v) override {
        LabeledField::add_to(v);
        v.push_back(_list);
    }
    void update() override;
    Layout::Ptr representative() override { return _list; }
};
struct LabeledPath : public LabeledField {
    class FileItem {
        GETTER(file::Path, path)
        
    public:
        FileItem(const file::Path& path = "");
        
        Color base_color() const;
        Color color() const;
        operator std::string() const;
        bool operator!=(const FileItem& other) const {
            return _path != other._path;
        }
    };
    
    gui::derived_ptr<gui::Dropdown> _dropdown;
    sprite::Reference _ref;
    
    std::vector<FileItem> _names;
    std::vector<Dropdown::TextItem> _search_items;
    std::set<file::Path, std::function<bool(const file::Path&, const file::Path&)>> _files;
    file::Path _path;
    std::function<bool(file::Path)> _validity;
    
    LabeledPath(std::string name, file::Path path = "");
    void add_to(std::vector<Layout::Ptr>& v) override {
        LabeledField::add_to(v);
        v.push_back(_dropdown);
    }
    void update() override;
    void update_names();
    void change_folder(const file::Path&);
    Layout::Ptr representative() override { return _dropdown; }
};
struct LabeledCheckbox : public LabeledField {
    gui::derived_ptr<gui::Checkbox> _checkbox;
    sprite::Reference _ref;
    LabeledCheckbox(const std::string& name = "");
    void add_to(std::vector<Layout::Ptr>& v) override {
        //LabeledField::add_to(v);
        v.push_back(_checkbox);
    }
    void update() override;
    Layout::Ptr representative() override { return _checkbox; }
    void set_description(std::string) override;
};

template <typename T> constexpr std::string_view type_name();

template <>
constexpr std::string_view type_name<void>()
{ return "void"; }

namespace detail {

using type_name_prober = void;

template <typename T>
constexpr std::string_view wrapped_type_name()
{
#ifdef __clang__
    return __PRETTY_FUNCTION__;
#elif defined(__GNUC__)
    return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
    return __FUNCSIG__;
#else
#error "Unsupported compiler"
#endif
}

constexpr std::size_t wrapped_type_name_prefix_length() {
    return wrapped_type_name<type_name_prober>().find(type_name<type_name_prober>());
}

constexpr std::size_t wrapped_type_name_suffix_length() {
    return wrapped_type_name<type_name_prober>().length()
        - wrapped_type_name_prefix_length()
        - type_name<type_name_prober>().length();
}

} // namespace detail

template <typename T>
constexpr std::string_view type_name() {
    constexpr auto wrapped_name = detail::wrapped_type_name<T>();
    constexpr auto prefix_length = detail::wrapped_type_name_prefix_length();
    constexpr auto suffix_length = detail::wrapped_type_name_suffix_length();
    constexpr auto type_name_length = wrapped_name.length() - prefix_length - suffix_length;
    return wrapped_name.substr(prefix_length, type_name_length);
}

//template <typename T>
//struct Variable : Variable<decltype(&T::operator())>
//{ };

//template<class T> class Variable;
template<class return_type, typename... arg_types>
class Variable;

template<class F, typename R = typename cmn::detail::return_type<F>::type, typename arg = typename  cmn::detail::arg_type<F>::type>
Variable(F&&) -> Variable<R, arg>;

template<typename... _Args>
class VarBase;

template <typename T>
struct arg_types : arg_types<decltype(&T::operator())>
{ };

template <typename ClassType, typename ReturnType, typename Arg, typename... Args>
struct arg_types<ReturnType(ClassType::*)(Arg, Args...) const>
{
    using type = Arg;
};

template <typename ClassType, typename ReturnType>
struct arg_types<ReturnType(ClassType::*)() const>
{
    using type = void;
};

template <typename T, typename R>
struct function_type : function_type<R, decltype(&T::operator())>
{ };

template <typename ClassType, typename R, typename ReturnType, typename... Args>
struct function_type<R, ReturnType(ClassType::*)(Args...) const>
{
    using type = std::function<R(Args...)>;
};


template<class return_type, typename... arg_types>
class Variable : public VarBase<arg_types...> {
public:
    //using return_type = typename cmn::detail::return_type<F>::type;//typename std::invoke_result_t<F>;
    //using arg_types = typename dyn::arg_types<F>::type;

private:
    static constexpr auto refcode = type_name<return_type>();

protected:
    std::function<return_type(arg_types...)> function;
    
public:
    using VarBase<arg_types...>::value_string;
    
public:
    constexpr Variable(std::function<return_type(arg_types...)> fn) noexcept
        :   VarBase<arg_types...>(refcode),
            function(std::move(fn))
    {
        value_string = [this](auto... args) -> std::string {
            if constexpr(std::same_as<std::string, return_type>)
                return function(std::forward<decltype(args)>(args)...);
            else if constexpr(_clean_same<sprite::Map&, return_type>) {
                auto& map = function(args...);
                
                auto access = [&map](std::string key) -> std::string {
                    auto ref = map[key];
                    if(ref.get().valid()) {
                        if(ref.template is_type<std::string>()
                           || ref.template is_type<file::Path>())
                        {
                            auto str = ref.get().valueString();
                            if(str.length() >= 2)
                                return str.substr(1, str.length()-2);
                        }
                        return ref.get().valueString();
                    }
                    throw std::invalid_argument("Cannot find given parameter.");
                };
                
                return ( access(args), ... );
                
            }
            else if constexpr(_clean_same<sprite::ConstReference, return_type> || _clean_same<sprite::Reference, return_type>) {
                auto r = function(std::forward<decltype(args)>(args)...);
                if(r.get().valid()) {
                    if constexpr(_clean_same<std::string, return_type>) {
                        auto str = r.get().valueString();
                        print("IS STRING: ", str);
                        return str.length() < 2 ? "" : str.substr(1, str.length()-2);
                    }
                    return r.get().valueString();
                } else
                    return "null";
            } else
                return Meta::toStr( function(std::forward<decltype(args)>(args)...) );
        };
    }

    template<typename... Args>
    constexpr return_type get(Args... args) const noexcept {
        return function(std::forward<Args>(args)...);
    }
};

template<typename... _Args>
class VarBase {
protected:
    const std::string_view type;
    
public:
    std::function<std::string(_Args...)> value_string;
    
public:
    VarBase(std::string_view type) : type(type) {}
    virtual ~VarBase() {}
    
    template<typename T>
    constexpr bool is() const noexcept {
        //return dynamic_cast<const Variable<T,  _Args...>*>(this) != nullptr;
        //
        
        return type == type_name<T>();
    }

    template<typename T>
    constexpr T value(_Args... args) const noexcept {
        using Target_t = const Variable<T, _Args...>*;
        static_assert(std::same_as<const VarBase<_Args...>*, decltype(this)>);
        
        assert(dynamic_cast<Target_t>(this) != nullptr);
        return static_cast<Target_t>(this)->get(std::forward<_Args>(args)...);
    }

    constexpr auto class_name() const { return type; }
};

ENUM_CLASS(LayoutType,
           each,
           condition,
           vlayout,
           hlayout,
           collection,
           button,
           text,
           stext,
           rect,
           textfield,
           checkbox,
           settings,
           combobox,
           image);

inline Color parse_color(const auto& obj) {
    if(not obj.is_array())
        throw std::invalid_argument("Color is not an array.");
    if(obj.size() < 3)
        throw std::invalid_argument("Color is not an array of RGB.");
    uint8_t a = 255;
    if(obj.size() >= 4) {
        a = obj[3].template get<uint8_t>();
    }
    return Color(obj[0].template get<uint8_t>(),
                 obj[1].template get<uint8_t>(),
                 obj[2].template get<uint8_t>(),
                 a);
}


using VarBase_t = VarBase<std::string>;
using VarReturn_t = sprite::Reference;

struct Context {
    std::unordered_map<std::string, std::function<void(Event)>> actions;
    std::unordered_map<std::string, std::shared_ptr<VarBase_t>> variables;
};

struct State;

struct LoopBody {
    std::string variable;
    nlohmann::json child;
    std::unique_ptr<State> state;
    std::vector<std::shared_ptr<VarBase_t>> cache;
};

struct IfBody {
    std::string variable;
    Layout::Ptr _if;
    Layout::Ptr _else;
};

struct State {
    size_t object_index{0};
    robin_hood::unordered_map<size_t, robin_hood::unordered_map<std::string, std::string>> patterns;
    std::unordered_map<size_t, std::function<void(DrawStructure&)>> display_fns;
    std::unordered_map<size_t, LoopBody> loops;
    std::unordered_map<size_t, IfBody> ifs;
    
    std::map<std::string, std::unique_ptr<LabeledField>> _text_fields;
    Layout::Ptr _settings_tooltip;
    std::unordered_map<std::string, std::tuple<size_t, Image::Ptr>> _image_cache;
    
    State() = default;
    State(State&&) = default;
    State(const State& other)
        : object_index(other.object_index),
          patterns(other.patterns),
          display_fns(other.display_fns),
          ifs(other.ifs)
    {
        for(auto &[k, body] : other.loops) {
            loops[k] = {
                .variable = body.variable,
                .child = body.child,
                .state = std::make_unique<State>(),//*body.state),
                .cache = body.cache
            };
        }
    }
    
    State& operator=(const State& other) = default;
    State& operator=(State&& other) = default;
};

namespace Modules {
struct Module {
    std::string _name;
    std::function<void(size_t, State&, const Layout::Ptr&)> _apply;
};

void add(Module&&);
void remove(const std::string& name);
Module* exists(const std::string& name);
}

Layout::Ptr parse_object(const nlohmann::json& obj,
                         const Context& context,
                         State& state);

std::string parse_text(const std::string& pattern, const Context& context);

struct VarProps {
    std::vector<std::string> parts;
    std::string sub;
    bool optional{false};
    bool html{false};
};

template<typename ApplyF, typename ErrorF>
inline auto resolve_variable(const std::string& word, const Context& context, ApplyF&& apply, ErrorF&& error) -> typename cmn::detail::return_type<ApplyF>::type {
    auto variable = utils::lowercase(word);
    VarProps props;
    
    std::regex rgx("[^a-zA-Z0-9_\\-: ]+");
    std::smatch match;
    std::string controls;
    if (std::regex_search(variable, match, rgx)) {
        controls = match[0].str();
        if (controls.find('.') == 0) {
            // optional variable
            props.optional = true;
        }
        if(controls.find('#') == 0) {
            props.html = true;
        }
        variable = variable.substr(controls.size());
    }
    
    props.parts = utils::split(variable, ':');
    variable = props.parts.front();
    
    std::string modifiers;
    if(props.parts.size() > 1)
        modifiers = props.parts.back();
    props.parts.erase(props.parts.begin());
    props.sub = modifiers;
    
    if(context.variables.contains(variable)) {
        if constexpr(std::invocable<ApplyF, VarBase_t&, const std::string&>)
            return apply(*context.variables.at(variable), modifiers);
        else if constexpr(std::invocable<ApplyF, VarBase_t&, const VarProps&>)
            return apply(*context.variables.at(variable), props);
        else
            static_assert(std::invocable<ApplyF, VarBase_t&, const VarProps&>);
    }
    
    if constexpr(std::invocable<ErrorF, bool>)
        return error(props.optional);
    else
        return error();
}

template<typename T>
T resolve_variable_type(std::string word, const Context& context) {
    if(word.empty())
        throw std::invalid_argument("Invalid variable type for "+word);
    
    if(word.length() > 2
       && word.front() == '{'
       && word.back() == '}') {
        word = word.substr(1,word.length()-2);
    }
    
    return resolve_variable(word, context, [&](const VarBase_t& variable, const std::string& modifiers) -> T
    {
        if constexpr(std::is_same_v<T, bool>) {
            if(variable.is<bool>()) {
                return variable.value<T>(modifiers);
            } else if(variable.is<file::Path>()) {
                auto tmp = variable.value<file::Path>(modifiers);
                return not tmp.empty();
            } else if(variable.is<std::string>()) {
                auto tmp = variable.value<std::string>(modifiers);
                return not tmp.empty();
            } else if(variable.is<sprite::Map&>()) {
                auto& tmp = variable.value<sprite::Map&>("null");
                auto ref = tmp[modifiers];
                if(not ref.get().valid()) {
                    FormatWarning("Retrieving invalid property (", modifiers, ") from map with keys ", tmp.keys(), ".");
                    return false;
                }
                //print(ref.get().name()," = ", ref.get().valueString(), " with ", ref.get().type_name());
                if(ref.template is_type<T>()) {
                    return ref.get().template value<T>();
                } else if(ref.is_type<std::string>()) {
                    return not ref.get().value<std::string>().empty();
                } else if(ref.is_type<file::Path>()) {
                    return not ref.get().value<file::Path>().empty();
                }
            }
        }
        
        if(variable.is<T>()) {
            return variable.value<T>(modifiers);
            
        } else if(variable.is<sprite::Map&>()) {
            auto& tmp = variable.value<sprite::Map&>("null");
            auto ref = tmp[modifiers];
            if(ref.template is_type<T>()) {
                return ref.get().template value<T>();
            }
            
        } else if(variable.is<VarReturn_t>()) {
            auto tmp = variable.value<VarReturn_t>(modifiers);
            if(tmp.template is_type<T>()) {
                return tmp.get().template value<T>();
            }
        }
        
        throw std::invalid_argument("Invalid variable type for "+word);
        
    }, [&]() -> T {
        throw std::invalid_argument("Invalid variable type for "+word);
    });
}

void update_objects(DrawStructure&, const Layout::Ptr& o, const Context& context, State& state);

inline tl::expected<nlohmann::json, const char*> load(const file::Path& path){
    static Timer timer;
    if(timer.elapsed() < 0.15) {
        return tl::unexpected("Have to wait longer to reload.");
    }
    timer.reset();
    
    auto text = path.read_file();
    static std::string previous;
    if(previous != text) {
        previous = text;
        return nlohmann::json::parse(text)["objects"];
    } else
        return tl::unexpected("Nothing changed.");
}

void update_layout(const file::Path&, Context&, State&, std::vector<Layout::Ptr>&);
void update_tooltips(DrawStructure&, State&);

}
}
