#include "Context.h"
#include <gui/dyn/State.h>
#include <gui/dyn/Action.h>
#include <gui/DynamicGUI.h>
#include <gui/dyn/ParseText.h>

namespace cmn::gui::dyn {

void CurrentObjectHandler::register_tooltipable(std::weak_ptr<LabeledField> field, std::weak_ptr<Drawable> ptr) {
    _textfields.emplace_back(field, ptr);
}

void CurrentObjectHandler::unregister_tooltipable(std::weak_ptr<LabeledField> field) {
    auto ptr = field.lock();
    if(not ptr)
        return; /// invalid input
    
    for(auto it = _textfields.begin(); it != _textfields.end();) {
        if(auto lock = std::get<0>(*it).lock();
           lock == nullptr)
        {
            it = _textfields.erase(it);
            
        } else if(lock == ptr) {
            _textfields.erase(it);
            return;
            
        } else {
            ++it;
        }
    }
}

void CurrentObjectHandler::update_tooltips(DrawStructure &graph) {
    std::shared_ptr<Drawable> found = nullptr;
    std::string name{"<null>"};
    std::unique_ptr<sprite::Reference> ref;
    
    for(auto it = _textfields.begin(); it != _textfields.end(); ) {
        auto &[_field, _ptr] = *it;
        auto ptr = _field.lock();
        
        if(not ptr) {
            it = _textfields.erase(it);
            continue;
        } else {
            ++it;
        }
        
        ptr->text()->set_clickable(true);
        
        const Drawable* hover_object = ptr->representative().get();
        if(hover_object
           && hover_object->type() == Type::ENTANGLED
           && static_cast<const Entangled*>(hover_object)->tooltip_object())
        {
            hover_object = static_cast<const Entangled*>(hover_object)->tooltip_object();
        }
        
        if(hover_object
           && hover_object->is_staged()
           && hover_object->hovered())
        {
            //found = ptr->representative();
            if(dynamic_cast<const LabeledCombobox*>(ptr.get())) {
                auto p = dynamic_cast<const LabeledCombobox*>(ptr.get());
                auto hname = p->highlighted_parameter();
                if(hname.has_value()) {
                    //Print("This is a labeled combobox: ", graph.hovered_object(), " with ", hname.value(), " highlighted.");
                    name = hname.value();
                    found = ptr->representative().get_smart();
                    break;
                } else {
                    //Print("This is a labeled combobox: ", graph.hovered_object(), " with nothing highlighted.");
                    hname = p->selected_parameter();
                    if(hname.has_value()) {
                        name = hname.value();
                        found = ptr->representative().get_smart();
                        break;
                    }
                }
            } else {
                found = ptr->representative().get_smart();
                auto r = ptr->ref();
                name = r.valid() ? r.name() : "<null>";
                break;
            }
            //if(ptr->representative().is<Combobox>())
            //ptr->representative().to<Combobox>()-
        }
    }
    
    if(found) {
        ref = std::make_unique<sprite::Reference>(GlobalSettings::write_value<NoType>(name));
    }
    
    const Drawable* hover_object = found.get();
    if(found
       && found->type() == Type::ENTANGLED
       && static_cast<const Entangled*>(found.get())->tooltip_object())
    {
        hover_object = static_cast<const Entangled*>(found.get())->tooltip_object();
    }
       

    if(found
       && ref
       && hover_object->hovered()
       && hover_object->is_staged())
    {
        //auto ptr = state._settings_tooltip.to<SettingsTooltip>();
        //if(ptr && ptr->other()
        //   && (not ptr->other()->parent() || not ptr->other()->parent()->stage()))
        {
            // dont draw
            //state._settings_tooltip.to<SettingsTooltip>()->set_other(nullptr);
        } //else {
        if(name.empty())
            Print("Error!");
        
        set_tooltip(name, found);
        add_tooltip(graph);
        //}
        
    } else {
        set_tooltip(nullptr);
    }
}

void CurrentObjectHandler::add_tooltip(DrawStructure &graph) {
    if(_tooltip_object) {
        if(auto lock = _tooltip_object->other().lock();
           lock && lock->is_staged() && lock->hovered())
        {
            graph.wrap_object(*_tooltip_object);
        } else {
            _tooltip_object = nullptr;
        }
    }
}

void CurrentObjectHandler::set_tooltip(const std::string_view &parameter, std::weak_ptr<Drawable> other) {
    if(not _tooltip_object)
        _tooltip_object = std::make_shared<SettingsTooltip>();
    _tooltip_object->set_other(other);
    _tooltip_object->set_parameter((std::string)parameter);
}

void CurrentObjectHandler::set_tooltip(std::nullptr_t) {
    _tooltip_object = nullptr;
}

void CurrentObjectHandler::set_variable_value(std::string_view name, std::string_view value)
{
    _variable_values[std::string(name)] = std::string(value);
}

void CurrentObjectHandler::remove_variable(std::string_view name)
{
    auto it = _variable_values.find(name);
    if(it != _variable_values.end()) {
        _variable_values.erase(it);
    }
}

std::optional<std::string_view> CurrentObjectHandler::get_variable_value(std::string_view name) const
{
    if(auto it = _variable_values.find(name);
       it != _variable_values.end())
    {
        return std::string_view(it->second);
    }
    
    return std::nullopt;
}

std::shared_ptr<Drawable> CurrentObjectHandler::retrieve_named(std::string_view name)
{
    if(auto it = _named_entities.find(name);
       it != _named_entities.end())
    {
        auto lock = it->second.lock();
        if(not lock)
            _named_entities.erase(it);
        return lock;
    }
    return nullptr;
}

void CurrentObjectHandler::register_named(const std::string &name, const std::shared_ptr<Drawable>& ptr)
{
    if(not ptr)
        throw InvalidArgumentException("Cannot register a null pointer for name ",name,".");
    ptr->on_delete([this, name, ptr = std::weak_ptr(ptr)]() {
        //Print("Deleting ", name, " from named entities.");
        if(auto it = _named_entities.find(name);
           it != _named_entities.end())
        {
            if(it->second.lock() == ptr.lock())
                _named_entities.erase(it);
#ifndef NDEBUG
            else
                Print("Error: ", name, " is not the same as the one in the map.");
#endif
        }
    });
    _named_entities[name] = ptr;
}

void CurrentObjectHandler::reset() {
    _current_object.reset();
}

void CurrentObjectHandler::select(const std::shared_ptr<Drawable> & ptr) {
    _current_object = std::weak_ptr(ptr);
}

std::shared_ptr<Drawable> CurrentObjectHandler::get() const {
    return _current_object.lock();
}

Context::Context(std::initializer_list<std::variant<ActionPair, VariablePair>> init_list)
{
    for (auto& item : init_list) {
        std::visit([this](auto& pair) mutable {
            using T = std::decay_t<decltype(pair)>;
            if constexpr(std::same_as<T, ActionPair>) {
                actions[pair.first] = std::move(pair.second);
            } else if constexpr(std::same_as<T, VariablePair>) {
                variables[pair.first] = std::move(pair.second);
                //variables.insert(pair);
            } else {
                static_assert(std::same_as<T, void>, "Unknown type");
            }
        }, item);
        
    }
}

bool Context::has(std::string_view name) const noexcept {
    if(variables.contains(name))
        return true;
    return system_variables().contains(name);
}

std::optional<decltype(Context::variables)::const_iterator> Context::find(std::string_view name) const noexcept
{
    auto it = variables.find(name);
    if(it != variables.end())
        return it;
    
    it = system_variables().find(name);
    if(it != system_variables().end())
        return it;
    return std::nullopt;
}

template<typename T, typename K = T>
K map_vectors(const VarProps& props, auto&& apply) {
    REQUIRE_EXACTLY(2, props);
    
    T A{}, B{};
    
    try {
        std::string_view a = utils::trim(std::string_view(props.parameters.front()));
        std::string_view b = utils::trim(std::string_view(props.parameters.back()));
        
        if constexpr(std::is_floating_point_v<std::remove_cvref_t<T>>) {
            A = T(Meta::fromStr<double>(a));
            B = T(Meta::fromStr<double>(b));
            
        } else if constexpr(are_the_same<bool, T>) {
            A = convert_to_bool(a);
            B = convert_to_bool(b);
            
        } else if constexpr(std::is_integral_v<std::remove_cvref_t<T>>) {
            A = T(Meta::fromStr<int64_t>(a));
            B = T(Meta::fromStr<int64_t>(b));
            
        } else {
            if (utils::beginsWith(a, '[') && utils::endsWith(a, ']'))
                A = Meta::fromStr<T>(a);
            else
                A = T(Meta::fromStr<double>(a));
            
            if (utils::beginsWith(b, '[') && utils::endsWith(b, ']'))
                B = Meta::fromStr<T>(b);
            else
                B = T(Meta::fromStr<double>(b));
        }

    } catch(const std::exception& ex) {
        throw InvalidSyntaxException("Cannot parse ", props,": ", ex.what());
        return K{};
    }
    
    return apply(A, B);
}

template<typename T>
concept numeric = std::is_arithmetic_v<T> && not are_the_same<T, bool>;

template<typename T, typename Apply>
    requires requires (T A, Apply apply) {
        { apply(A) } -> std::convertible_to<T>;
    }
T map_vector(const VarProps& props, Apply&& apply) {
    REQUIRE_EXACTLY(1, props);
    
    T A{};
    
    try {
        std::string_view a = utils::trim(std::string_view(props.parameters.front()));
        
        if constexpr(std::is_floating_point_v<std::remove_cvref_t<T>>) {
            A = T(Meta::fromStr<double>(a));
            
        } else {
            if (utils::beginsWith(a, '[') && utils::endsWith(a, ']'))
                A = Meta::fromStr<T>(a);
            else
                A = T(Meta::fromStr<double>(a));
        }

    } catch(const std::exception& ex) {
        throw InvalidSyntaxException("Cannot parse ", props,": ", ex.what());
    }
    
    return apply(A);
}

template<typename T, typename Apply>
    requires requires (T A, T B, Apply apply) {
        { apply(A, B) } -> std::convertible_to<T>;
    }
T map_vector(const VarProps& props, Apply&& apply) {
    REQUIRE_EXACTLY(2, props);
    
    T A{}, B{};
    
    try {
        std::string_view a(props.parameters.front());
        std::string_view b(props.parameters.back());
        
        if constexpr(std::is_floating_point_v<std::remove_cvref_t<T>>) {
            A = T(Meta::fromStr<double>(a));
            B = T(Meta::fromStr<double>(b));
            
        } else {
            if (utils::beginsWith(a, '[') && utils::endsWith(a, ']'))
                A = Meta::fromStr<T>(a);
            else
                A = T(Meta::fromStr<double>(a));
            
            if (utils::beginsWith(b, '[') && utils::endsWith(b, ']'))
                B = Meta::fromStr<T>(b);
            else
                B = T(Meta::fromStr<double>(b));
        }

    } catch(const std::exception& ex) {
        throw InvalidSyntaxException("Cannot parse ", props,": ", ex.what());
    }
    
    return apply(A, B);
}


void Context::init() const {
    if(not _system_variables.has_value())
        _system_variables = {
            VarFunc("repeat", [](const VarProps& props) -> std::string {
                REQUIRE_EXACTLY(2, props);
                return utils::repeat(props.last(), Meta::fromStr<uint8_t>(props.first()));
            }),
            VarFunc("mouse", [this](const VarProps&) -> Vec2 {
                return _last_mouse;
            }),
            VarFunc("time", [](const VarProps&) -> double {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() );
                return static_cast<double>(ms.count()) / 1000.0;
            }),
            VarFunc("min", [](const VarProps& props) {
                return map_vectors<double>(props, [](auto&A, auto&B){return min(A, B);});
            }),
            VarFunc("max", [](const VarProps& props) {
                return map_vectors<double>(props, [](auto&A, auto&B){return max(A, B);});
            }),
            
            VarFunc("saturate", [](const VarProps& props) {
                REQUIRE_EXACTLY(3, props);
                auto v = Meta::fromStr<double>(props.parameters.at(0));
                auto mi = Meta::fromStr<double>(props.parameters.at(1));
                auto ma = Meta::fromStr<double>(props.parameters.at(2));
                return saturate(v, mi, ma);
            }),
            VarFunc("bool", [](const VarProps& props) -> bool {
                REQUIRE_EXACTLY(1, props);
                return convert_to_bool(props.parameters.front());
            }),
            VarFunc("abs", [](const VarProps& props) {
                REQUIRE_EXACTLY(1, props);
                auto v = Meta::fromStr<double>(props.parameters.front());
                if(std::isnan(v))
                    return v;
                return fabs(v);
            }),
            VarFunc("int", [](const VarProps& props) -> int64_t {
                REQUIRE_EXACTLY(1, props);
                
                auto v = Meta::fromStr<double>(props.parameters.front());
                if(std::isnan(v))
                    return static_cast<int64_t>(std::numeric_limits<int64_t>::quiet_NaN());
                else
                    return static_cast<int64_t>(v);
            }),
            VarFunc("dec", [](const VarProps& props) -> std::string {
                REQUIRE_EXACTLY(2, props);
                float decimals = Meta::fromStr<uint8_t>(props.parameters.front());
                
                auto str = props.parameters.back();
                auto it = std::find(str.begin(), str.end(), '.');
                if(it != str.end()) {
                    size_t offset = 0;
                    while(++it != str.end() && offset++ < decimals) {}
                    str.erase(it, str.end());
                }
                
                return str;
            }),
            VarFunc("not", [](const VarProps& props) -> bool {
                REQUIRE_EXACTLY(1, props);
                
                std::string_view p(props.parameters.front());
                try {
                    return not convert_to_bool(p);
                    //return not Meta::fromStr<bool>(p);
                    
                } catch(const std::exception& ex) {
                    throw InvalidArgumentException("Cannot parse boolean ", p, ": ", ex.what());
                }
            }),
            VarFunc("equal", [](const VarProps& props) -> bool {
                REQUIRE_EXACTLY(2, props);
                
                std::string_view p0(props.parameters.front());
                std::string_view p1(props.parameters.back());
                try {
                    return p0 == p1;
                    
                } catch(const std::exception& ex) {
                    throw InvalidArgumentException("Cannot parse boolean ", p0, " == ",p1,": ", ex.what());
                }
            }),
            VarFunc("nequal", [](const VarProps& props) -> bool {
                REQUIRE_EXACTLY(2, props);
                
                std::string_view p0(props.parameters.front());
                std::string_view p1(props.parameters.back());
                try {
                    return not (p0 == p1);
                    
                } catch(const std::exception& ex) {
                    throw InvalidArgumentException("Cannot parse boolean ", p0, " == ",p1,": ", ex.what());
                }
            }),
            VarFunc("sqr", [](const VarProps& props) -> double {
                REQUIRE_EXACTLY(1, props);
                auto A = Meta::fromStr<double>(props.first());
                return SQR(A);
            }),
            VarFunc("&&", [](const VarProps& props) -> bool {
                for (auto &c : props.parameters) {
                    if(not convert_to_bool(c))
                        return false;
                }
                return true;
            }),
            VarFunc("||", [](const VarProps& props) -> bool {
                for (auto &c : props.parameters) {
                    if(convert_to_bool(c))
                        return true;
                }
                return false;
            }),
            VarFunc(">", [](const VarProps& props) {
                return map_vectors<double>(props, [](auto&A, auto&B){return A > B;});
            }),
            VarFunc(">=", [](const VarProps& props) {
                return map_vectors<double>(props, [](auto&A, auto&B){return A >= B;});
            }),
            VarFunc("<", [](const VarProps& props) {
                return map_vectors<double>(props, [](auto&A, auto&B){return A < B;});
            }),
            VarFunc("<=", [](const VarProps& props) {
                return map_vectors<double>(props, [](auto&A, auto&B){return A <= B;});
            }),
            VarFunc("+", [](const VarProps& props) {
                return map_vectors<double>(props, [](auto&A, auto&B){return A+B;});
            }),
            VarFunc("-", [](const VarProps& props) {
                return map_vectors<double>(props, [](auto&A, auto&B){return A-B;});
            }),
            VarFunc("*", [](const VarProps& props) {
                return map_vectors<double>(props, [](auto&A, auto&B){return A * B;});
            }),
            VarFunc("/", [](const VarProps& props) {
                return map_vectors<double>(props, [](auto&A, auto&B){return A / B;});
            }),
            VarFunc("lower", [](const VarProps& props) -> std::string {
                return utils::lowercase(props.first());
            }),
            VarFunc("upper", [](const VarProps& props) -> std::string {
                return utils::uppercase(props.first());
            }),
            VarFunc("cmap", [](const VarProps& props) -> Color {
                REQUIRE_EXACTLY(2, props);
                using namespace cmn::cmap;
                
                auto cmap = CMaps::Class::fromStr(utils::lowercase(props.first()));
                return cmap::ColorMap::value(cmap, Meta::fromStr<double>(props.last()));
            }),
            VarFunc("meanVector", [](const VarProps& props) -> Vec2 {
                REQUIRE_EXACTLY(1, props);
                auto vector = Meta::fromStr<std::vector<Vec2>>(props.first());
                Vec2 mean;
                for(auto &v : vector)
                    mean += v;
                mean /= vector.size();
                return mean;
            }),
            VarFunc("distance", [](const VarProps& props) -> Float2_t {
                REQUIRE_EXACTLY(2, props);
                auto A = Meta::fromStr<Vec2>(props.first());
                auto B = Meta::fromStr<Vec2>(props.last());
                return euclidean_distance(A, B);
            }),
            VarFunc("join", [](const VarProps& props) -> std::string {
                REQUIRE_AT_LEAST(1, props);
                
                auto& array = props.parameters.back();
                auto parts = util::parse_array_parts(util::truncate(utils::trim(array)));
                
                if(props.parameters.size() == 2) {
                    
                }
                
                std::stringstream ss;
                for(auto &p : parts) {
                    ss << p;
                }
                return ss.str();
            }),
            VarFunc("concat", [](const VarProps& props) -> std::vector<std::string> {
                REQUIRE_AT_LEAST(2, props);
                
                std::vector<std::string> combination;
                for(auto &p : props.parameters) {
                    auto parts = Meta::fromStr<std::vector<std::string>>(p);
                    combination.insert(combination.end(),
                                       std::make_move_iterator(parts.begin()),
                                       std::make_move_iterator(parts.end()));
                }
                
                return combination;
            }),
            VarFunc("for", [this](const VarProps& props) -> std::string {
                REQUIRE_EXACTLY(2, props);
                
                std::stringstream ss;
                ss << "[";
                
                auto trunc = props.parameters.at(0);
                auto fn = props.parameters.at(1);
                
                State state;
                auto handler = std::make_shared<CurrentObjectHandler>();
                state._current_object_handler = handler;
                
                if(is_in(utils::trim(trunc).front(), '[', '{')) {
                    auto parsed = parse_text(trunc, *this, state);
                    auto parts = util::parse_array_parts(util::truncate(parsed));
                    
                    size_t index{0};
                    for(auto &p : parts) {
                        handler->set_variable_value("i", p);
                        if(index > 0)
                            ss << ",";
                        ss << parse_text(fn, *this, state);
                        
                        ++index;
                    }
                    handler->remove_variable("i");
                    
                } else if(is_in(trunc.front(), '\'', '"')) {
                    auto text = Meta::fromStr<std::string>(trunc);
                    size_t index{0};
                    for(auto p : text) {
                        handler->set_variable_value("i", std::string_view(&p, 1));
                        if(index > 0)
                            ss << ",";
                        ss << parse_text(fn, *this, state);
                        
                        ++index;
                    }
                    handler->remove_variable("i");
                }
                
                ss << "]";
                //Print("=> ", no_quotes(ss.str()));
                return ss.str();
            }),
            VarFunc("addVector", [](const VarProps& props) -> Vec2 {
                if(props.parameters.size() == 2) {
                    return map_vectors<Vec2>(props, [](auto& A, auto& B){ return A + B; });
                } else {
                    Vec2 result;
                    for(auto &p : props.parameters) {
                        result += Meta::fromStr<Vec2>(p);
                    }
                    return result;
                }
            }),
            VarFunc("subVector", [](const VarProps& props) -> Vec2 {
                return map_vectors<Vec2>(props, [](auto& A, auto& B){ return A - B; });
            }),
            VarFunc("distance", [](const VarProps& props) -> Float2_t {
                return map_vectors<Vec2, Float2_t>(props, [](auto& A, auto& B) { return euclidean_distance(A, B); });
            }),
            VarFunc("mulVector", [](const VarProps& props) -> Vec2 {
                return map_vectors<Vec2>(props, [](auto& A, auto& B){ return A.mul(B); });
            }),
            VarFunc("divVector", [](const VarProps& props) -> Vec2 {
                return map_vectors<Vec2>(props, [](auto& A, auto& B){ return A.div(B); });
            }),
            VarFunc("mulSize", [](const VarProps& props) -> Size2 {
                return map_vectors<Size2>(props, [](auto& A, auto& B){ return A.mul(B); });
            }),
            VarFunc("divSize", [](const VarProps& props) -> Size2 {
                return map_vectors<Size2>(props, [](auto& A, auto& B){ return A.div(B); });
            }),
            VarFunc("minVector", [](const VarProps& props) -> Vec2 {
                return map_vectors<Vec2>(props, [](auto& A, auto& B){ return cmn::min(A, B); });
            }),
            VarFunc("addSize", [](const VarProps& props) -> Size2 {
                return map_vectors<Size2>(props, [](auto& A, auto& B){ return A + B; });
            }),
            VarFunc("round", [](const VarProps& props) {
                if(props.parameters.size() > 1) {
                    auto p = props;
                    auto decimalPlaces = Meta::fromStr<uint8_t>(props.parameters.back());
                    double scale = std::pow(10.0, decimalPlaces);
                    assert(p.parameters.size() > 1);
                    p.parameters.pop_back();
                    return map_vector<double>(p, [scale](auto& A){ return std::round(A * scale) / scale; });
                }
                return map_vector<double>(props, [](auto& A){ return std::round(A); });
            }),
            VarFunc("mod", [](const VarProps& props) -> int {
                return map_vector<double>(props, [](auto& A, auto& B){ return int64_t(A) % int64_t(B); });
            }),
            VarFunc("empty", [](const VarProps& props) -> bool {
                REQUIRE_EXACTLY(1, props);
                auto trunc = utils::trim(props.parameters.front());
                if(is_in(trunc.front(), '[', '{')) {
                    return util::parse_array_parts(util::truncate(trunc)).empty();
                }
                
                throw InvalidArgumentException("Argument in ", props, " not indexable.");
            }),
            VarFunc("at", [](const VarProps& props) -> std::string {
                REQUIRE_EXACTLY(2, props);
                
                auto index = Meta::fromStr<size_t>(props.parameters.front());
                auto trunc = props.parameters.at(1);
                if(is_in(utils::trim(trunc).front(), '[', '{')) {
                    auto parts = util::parse_array_parts(util::truncate(utils::trim(trunc)));
                    return parts.at(index);
                } else if(is_in(trunc.front(), '\'', '"')) {
                    return std::string(1, Meta::fromStr<std::string>(trunc).at(index));
                }
                
                throw InvalidArgumentException("Needs to be an indexable type.");
            }),
            VarFunc("array_length", [](const VarProps& props) -> size_t {
                REQUIRE_EXACTLY(1, props);
                
                auto trunc = props.parameters.at(0);
                if(is_in(utils::trim(trunc).front(), '[', '{')) {
                    auto parts = util::parse_array_parts(util::truncate(utils::trim(trunc)));
                    return parts.size();
                } else if(is_in(trunc.front(), '\'', '"')) {
                    return Meta::fromStr<std::string>(trunc).length();
                }
                
                throw InvalidArgumentException("Needs to be an indexable type.");
            }),
            VarFunc("substr", [](const VarProps& props) -> std::string {
                if(props.parameters.size() < 2) {
                    throw InvalidArgumentException("Invalid number of variables for at: ", props);
                }
                
                uint32_t from = 0;
                uint32_t to = 0;
                from = Meta::fromStr<uint32_t>(props.parameters.at(0));
                if(props.parameters.size() == 3) {
                    to = Meta::fromStr<uint32_t>(props.parameters.at(1));
                    if(to > from) {
                        to -= from;
                    } else
                        to = 0;
                }
                
                auto trunc = props.parameters.back();
                if(is_in(trunc.front(), '\'', '"')) {
                    trunc = Meta::fromStr<std::string>(trunc);
                }
                if(trunc.length() <= from)
                    return "";
                return trunc.substr(from, to);
            }),
            VarFunc("pad_string", [](const VarProps& props) -> std::string {
                if(props.parameters.size() < 2) {
                    throw InvalidArgumentException("Invalid number of variables for at: ", props);
                }
                
                uint32_t L = Meta::fromStr<uint32_t>(props.parameters.at(0));
                auto trunc = props.parameters.back();
                
                if(is_in(trunc.front(), '\'', '"')) {
                    trunc = Meta::fromStr<std::string>(trunc);
                }
                
                if(L == 0)
                    return trunc;
                
                if(trunc.length() < L) {
                    if(props.parameters.size() == 3) {
                        auto c = props.parameters.at(1);
                        return trunc + utils::repeat(c, L - trunc.length());
                    }
                    return trunc + utils::repeat(" ", L - trunc.length());
                } else
                    return trunc;
            }),
            VarFunc("padl_string", [](const VarProps& props) -> std::string {
                if(props.parameters.size() < 2) {
                    throw InvalidArgumentException("Invalid number of variables for at: ", props);
                }
                
                uint32_t L = Meta::fromStr<uint32_t>(props.parameters.at(0));
                auto trunc = props.parameters.back();
                
                if(is_in(trunc.front(), '\'', '"')) {
                    trunc = Meta::fromStr<std::string>(trunc);
                }
                
                if(L == 0)
                    return trunc;
                
                if(trunc.length() < L) {
                    if(props.parameters.size() == 3) {
                        auto c = props.parameters.at(1);
                        return utils::repeat(c, L - trunc.length()) + trunc;
                    }
                    return utils::repeat(" ", L - trunc.length()) + trunc;
                } else
                    return trunc;
            }),
            VarFunc("global", [](const VarProps& props) -> std::string {
                if(props.subs.empty())
                    return "null";
                auto v = GlobalSettings::read_value<NoType>(props.subs.front());
                if(v.valid()) {
                    if(v->is_type<std::string>()) {
                        return v->value<std::string>();
                    } else if(v->is_type<file::Path>()) {
                        return v->value<file::Path>().str();
                    }
                    return v->valueString();
                }
                return "null";
            }),
            VarFunc("clrAlpha", [](const VarProps& props) -> Color {
                REQUIRE_EXACTLY(2, props);
                
                try {
                    auto color = Meta::fromStr<Color>(props.parameters.front());
                    auto alpha = Meta::fromStr<float>(props.parameters.back());
                    
                    return color.alpha(alpha);
                    
                } catch(const std::exception& ex) {
                    throw InvalidArgumentException("Cannot parse color ", props.parameters.front(), " and alpha ", props.parameters.back(), ": ", ex.what());
                }
            }),
            
            VarFunc("shorten", [](const VarProps& props) -> std::string {
                if(props.parameters.size() < 1 || props.parameters.size() > 3) {
                    throw InvalidArgumentException("Need one to three arguments for ", props,".");
                }
                
                size_t N = 50;
                std::string_view placeholder = "…";
                if(props.parameters.size() > 1)
                    N = Meta::fromStr<size_t>(props.parameters.at(1));
                if(props.parameters.size() > 2)
                    placeholder = props.last();
                
                return utils::ShortenText(props.first(), N, 0.5, placeholder);
            }),
            VarFunc("filename", [](const VarProps& props) -> file::Path {
                REQUIRE_EXACTLY(1, props);
                return file::Path(Meta::fromStr<file::Path>(props.first()).filename());
            }),
            VarFunc("folder", [](const VarProps& props) -> file::Path {
                REQUIRE_EXACTLY(1, props);
                return file::Path(Meta::fromStr<file::Path>(props.first()).remove_filename());
            }),
            VarFunc("basename", [](const VarProps& props) -> file::Path {
                REQUIRE_EXACTLY(1, props);
                return file::Path(Meta::fromStr<file::Path>(props.first()).filename()).remove_extension();
            })
        };
    
}

const std::shared_ptr<VarBase_t>& Context::variable(std::string_view name) const {
    auto it = variables.find(name);
    if(it != variables.end())
        return it->second;
    
    init();
    
    auto sit = system_variables().find(name);
    if(sit != system_variables().end())
        return sit->second;
    
    throw InvalidArgumentException("Cannot find key ", name, " in variables.");
}

}
