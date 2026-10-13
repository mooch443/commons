/**
 * Project Untitled
 */

#include "GlobalSettings.h"
#include <misc/Path.h>

namespace cmn {

std::mutex& GlobalSettings::instance_mutex() {
    static std::mutex _mutex;
    return _mutex;
}

GlobalSettings*& GlobalSettings::instance_storage() {
    static GlobalSettings* _instance{nullptr};
    return _instance;
}

GlobalSettings& GlobalSettings::create() {
    static GlobalSettings _owned_instance{};
    instance(&_owned_instance);
    return _owned_instance;
}

bool& GlobalSettings::is_reading_config_flag() {
    static thread_local bool flag{false};
    return flag;
}

bool& GlobalSettings::is_writing_config_flag() {
    static thread_local bool flag{false};
    return flag;
}

void GlobalSettings::set_instance(GlobalSettings* ptr) {
    instance(ptr);
}

std::shared_mutex& GlobalSettings::mutex() {
    static std::shared_mutex _mutex;
    return _mutex;
}

std::shared_mutex& GlobalSettings::defaults_mutex() {
    static std::shared_mutex _mutex;
    return _mutex;
}

/**
 * GlobalSettings implementation
 */

GlobalSettings::GlobalSettings() {
    _config.values.set_print_by_default(false);
    _config.defaults.set_print_by_default(false);
    
    _config.values.register_before_write_callback([this](std::string_view name) {
        GlobalSettings::write([&](Configuration&) {
            _config.history.before_value_changed(_config._access_level(name), _config.values, name);
        });
    });
    _config.values.register_general_callback([this](std::string_view name) {
        GlobalSettings::write([&](Configuration&) {
            _config.history.value_changed(_config._access_level(name), _config.values, name);
        });
    });
}

void ParameterHistory::ignore(std::initializer_list<std::string_view> values) {
    _ignored_names.insert(values.begin(), values.end());
}

bool ParameterHistory::matches_ignored_name(std::string_view name) const {
    return _ignored_names.contains(name);
}

bool ParameterHistory::matches_ignored_prefix(std::string_view name) const {
    for(const auto &prefix : _ignored_prefixes) {
        if(utils::beginsWith(name, prefix)) {
            return true;
        }
    }
    return false;
}

HistoryItem& ParameterHistory::last() {
    if(_index == 0u)
        throw InvalidArgumentException("No last item (history is empty).");
    return _items.at(_index - 1u);
}

const HistoryItem& ParameterHistory::last() const {
    if(_index == 0u)
        throw InvalidArgumentException("No last item (history is empty).");
    return _items.at(_index - 1u);
}

std::optional<std::tuple<std::chrono::microseconds, size_t>> ParameterHistory::time_since(std::optional<std::string_view> name) const {
    if(_index == 0u) {
        return std::nullopt;
    }
    
    auto now = timestamp_t::now();
    if(not name) {
        return std::make_tuple((std::chrono::microseconds)now - (std::chrono::microseconds)last().timestamp, _index - 1u);
    }
    
    const auto end = rend();
    size_t index = 0;
    for(auto rit = rbegin(); rit != end; ++rit, ++index)
    {
        if(rit->values.has(*name)) {
            return std::make_tuple((std::chrono::microseconds)now - (std::chrono::microseconds)rit->timestamp, _index - 1u - index);
        }
    }
    
    return std::nullopt;
}

void ParameterHistory::clear() {
    _items.clear();
    _index = 0;
}

std::vector<HistoryItem>::const_iterator ParameterHistory::begin() const {
    return _items.begin();
}
std::vector<HistoryItem>::const_iterator ParameterHistory::end() const {
    return _items.begin() + _index;
}
std::vector<HistoryItem>::const_reverse_iterator ParameterHistory::rbegin() const {
    return std::make_reverse_iterator(end());
}
std::vector<HistoryItem>::const_reverse_iterator ParameterHistory::rend() const {
    return std::make_reverse_iterator(begin());
}

std::optional<size_t> ParameterHistory::last_changed(std::string_view key) const {
    if(_index == 0u)
        return std::nullopt;
    
    const size_t index_start = _index - 1u;
    size_t index = index_start;
    const auto end = rend();
    for(auto rit = rbegin(); rit != end; ++rit, --index)
    {
        if(rit->values.has(key)) {
            return index_start - index;
        }
    }
    
    return std::nullopt;
}

void ParameterHistory::set_monitor_level(AccessLevel level) {
    _monitor_level = level;
}

void ParameterHistory::before_value_changed(AccessLevel level, sprite::Map& source, std::string_view name) {
    if(locked()
       || level > _monitor_level
       || matches_ignored_prefix(name)
       || matches_ignored_name(name))
    {
        /// we are probably inside an undo operation
#ifndef NDEBUG
        if(matches_ignored_prefix(name)) {
            Print(name, " matches an ignored prefix (", _ignored_prefixes, ")");
        }
        else if(matches_ignored_name(name)) {
            Print(name, " matches an ignored name (", _ignored_names, ")");
        }
        else if(not locked()) {
            Print("Level ", level, " is beyond monitor level ", _monitor_level, " for ", name);
        }
#endif
        return;
    }
    
    const auto found = last_value_of(name);
    if(not found) {
        /// apparently there was a settings reset at some point
        /// and we will have to preserve the previous value.
        prefixed_print("history", "Uninitialized value found for ", name);
        
        if(_items.empty()) {
            auto& item = push();
            item.timestamp = 0;
        }
        
        auto& item = _items.front();
        source.at(name).get().copy_to(item.values);
    }
}

void ParameterHistory::value_changed(AccessLevel level, sprite::Map& source, std::string_view name) {
    if(locked()
       || level > _monitor_level
       || matches_ignored_prefix(name)
       || matches_ignored_name(name))
    {
        /// we are probably inside an undo operation
#ifndef NDEBUG
        if(matches_ignored_prefix(name)) {
            Print(name, " matches an ignored prefix (", _ignored_prefixes, ")");
        }
        else if(matches_ignored_name(name)) {
            Print(name, " matches an ignored name (", _ignored_names, ")");
        }
        else if(not locked()) {
            Print("Level ", level, " is beyond monitor level ", _monitor_level, " for ", name);
        }
#endif
        return;
    }
    
    HistoryItem* item{nullptr};
    
    if(_index > 0u) {
        auto result = time_since(name);
        std::chrono::microseconds diff;
        item = &last();
        
        /*if(result) {
            diff = std::get<0>(*result);
        } else {*/
            diff = std::get<0>(*time_since(std::nullopt));
        //}
        
        /// if 25ms have passed, add a new history item - otherwise, just replace the existing one. this deals with recursion as well as combining packages of combined changed parameters.
        if(diff >= merge_time_threshold) {
            Print("Pushing new item for ", name, " with diff ", DurationUS(timestamp_t{diff}), " to ", result ? std::get<1>(*result) : 0u);
            item = &push(); /// update pointer to next item
        }
        /// otherwise we can just reuse the current item
        
        Print("Difference ", DurationUS(timestamp_t{diff}), " since last history vs. threshold = ", DurationUS(timestamp_t{merge_time_threshold}), " ", diff >= merge_time_threshold);
    } else {
        /// we're empty so just push a new one
        item = &push();
    }
    
    /// actually copy the value over
    source.at(name)->copy_to(item->values);
}

void ParameterHistory::print() const {
    std::string prefix = "  ";
    prefixed_print("history", "history (",_items.size(),")");
    for(size_t i = 0; i<_items.size(); ++i) {
        auto &item = _items.at(i);
        auto keys = item.values.keys();
        prefixed_print("history", prefix.c_str(), i >= _index ? " + " : " - ", i);
        for(auto& key : keys) {
            prefixed_print("history", prefix.c_str(), "   | ", key, " = ", no_quotes(item.values.at(key).get().valueString()));
        }
    }
}

void ParameterHistory::reset(AccessLevel level) {
    clear();
    _ignored_prefixes.clear();
    _ignored_names.clear();
    _monitor_level = level;
}

void ParameterHistory::ignore_prefix(std::string_view prefix) {
    _ignored_prefixes.insert(prefix);
}

std::optional<sprite::ConstReference> ParameterHistory::last_value_of(std::string_view key) const
{
    const auto end = rend();
    for(auto rit = rbegin(); rit != end; ++rit) {
        if(rit->values.has(key)) {
            return rit->values.at(key);
        }
    }
    
    return std::nullopt;
}

std::optional<sprite::ConstReference> ParameterHistory::historic_value_of(std::string_view key) const
{
    const auto end = rend();
    size_t count{0u};
    for(auto rit = rbegin(); rit != end; ++rit) {
        if(rit->values.has(key)) {
            if(count++ >= 1) {
                return rit->values.at(key);
            }
        }
    }
    
    return std::nullopt;
}

bool ParameterHistory::locked() const {
    return _locked.load();
}

void ParameterHistory::lock() {
    bool e{false};
    if(_locked.compare_exchange_strong(e, true)) {
        /// good
    } else {
        throw InvalidArgumentException("Was already locked.");
    }
}

void ParameterHistory::unlock() {
    bool e{true};
    if(_locked.compare_exchange_strong(e, false)) {
        /// good
    } else {
        throw InvalidArgumentException("Was not locked.");
    }
}

const HistoryItem& ParameterHistory::pop() {
    if(empty())
        throw InvalidArgumentException("Cannot pop empty history.");
    
    assert(_index > 0);
    --_index;
    assert(_index < _items.size());
    return _items.at(_index);
}

HistoryItem& ParameterHistory::push() {
    ++_index;
    
    /// at this point we will also cut off any "undone" history
    /// since we are pushing a new item... so it makes that other
    /// future invalid.
    _items.resize(_index);
    
    /// update the timestamp here
    _items.back().timestamp = timestamp_t::now();
    return _items.back();
}

bool ParameterHistory::empty() const noexcept {
    return _index <= 1u;
}
size_t ParameterHistory::size() const {
    if(_index == 0u)
        return 0u;
    return _index - 1u;
}

bool ParameterHistory::can_redo() const noexcept {
    /// we have some things that we undid if the index points
    /// to something earlier than the end of the vector
    return _index < _items.size();
}

std::optional<std::set<std::string>> ParameterHistory::redo(sprite::Map &target)
{
    if(not can_redo())
        return std::nullopt;
    
    ++_index;
    
    auto& item = last();
    auto keys = item.values.keys();
    
    run_locked([&](){
        std::map<std::string, std::string> current_values, next_values;
        for(auto key : keys) {
            next_values[key] = Meta::fromStr<std::string>(item.values.at(key).get().valueString());
            current_values[key] = Meta::fromStr<std::string>(target.at(key).get().valueString());
        }
        prefixed_print("history", "redoing ", current_values," -> ", next_values);
        
        for(auto key : keys) {
            item.values.at(key).get().copy_to(target);
        }
    });
    
    return std::set<std::string>{keys.begin(), keys.end()};
}

std::optional<std::set<std::string>> ParameterHistory::undo(sprite::Map &target)
{
    if(_index <= 1u)
        return std::nullopt;
    
    // these keys are about to be changed by the item we are
    // reseting to:
    // _index - 1 == current state of affairs
    //  => need to go one further
    //(void)pop();
    
    auto &item = last();
    auto keys = item.values.keys();
    
    sprite::Map next_map;
    std::map<std::string, std::string> current_values, next_values;
    for(auto key : keys) {
        auto historic = historic_value_of(key);
        if(historic) {
            historic->get().copy_to(next_map);
            next_values[key] = Meta::fromStr<std::string>(historic->get().valueString());
        } else {
            prefixed_print("history", "Have no historic value for ", key);
        }
        
        current_values[key] = Meta::fromStr<std::string>(target.at(key).get().valueString());
    }
    
    prefixed_print("history", "undoing ", current_values," -> ", next_values);
    (void)pop();

    run_locked([&](){
        /*if(_index == _items.size())
        {
            std::map<std::string_view, std::string> changed_keys;
            /// we are at the end of the histories
            /// so we need to insert an item for current values
            auto &item = push();
            for(auto key : keys) {
                changed_keys[key] = Meta::fromStr<std::string>(target.at(key).get().valueString());
                target.at(key).get().copy_to(item.values);
            }
            /// we go back to where we started...
            (void)pop();
            
            prefixed_print("history", "Inserting history for current elements: ", changed_keys);
        } else {
            /// make sure we are saving current values
            /// of the keys we are about to change...
            auto& item = last();
            for(auto key : keys) {
                if(item.values.has(key)) {
                    prefixed_print("history", "Already have value for ", item.values.at(key), " when trying to save ", target.at(key), " from current state.");
                }
                target.at(key).get().copy_to(item.values);
            }
        }*/
        
        /// pop one item and paste its values
        /// into the current target...
        for(auto key : next_map.keys()) {
            //Print("* copying ", key, " from ", next_map.at(key), " => ", target.at(key));
            next_map.at(key).get().copy_to(target);
        }
    });
    
    return std::set<std::string>{keys.begin(), keys.end()};
}

std::optional<std::set<std::string>> Configuration::undo() {
    return history.undo(this->values);
}

std::optional<std::set<std::string>> Configuration::redo() {
    return history.redo(this->values);
}

/**
 * Destructor of @class GlobalSettings.
 */
GlobalSettings::~GlobalSettings() {
}

void GlobalSettings::reset() {
    create() = GlobalSettings{};
    create();
}

bool GlobalSettings::is_runtime_quiet(const sprite::Map* ptr) {
    if(ptr)
        return ptr->has("quiet") && ptr->at("quiet").value<bool>();
    else {
        auto v = read_value<bool>("quiet");
        if(v)
            return *v;
        return false;
    }
}

std::shared_ptr<const sprite::PropertyType> GlobalSettings::at(std::string_view name) {
    return *read_value<NoType>(name);
}

std::optional<sprite::Map> GlobalSettings::current_defaults(std::string_view name) {
    std::unique_lock guard(defaults_mutex());
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    if(instance()->_current_defaults.has(name)) {
        sprite::Map tmp;
        instance()->_current_defaults.at(name).get().copy_to(tmp);
        return tmp;
    }

    return std::nullopt;
}

void GlobalSettings::current_defaults(std::string_view name, const sprite::Map& source) {
    std::unique_lock guard(defaults_mutex());
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    
    if(not source.has(name)) {
        throw U_EXCEPTION("Cannot find key ", name, " in source.");
    }
        
    source.at(name).get().copy_to(instance()->_current_defaults);
}

/*sprite::Map GlobalSettings::get_current_defaults() {
    std::unique_lock guard(defaults_mutex());
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    return instance()->_current_defaults;
}*/

void GlobalSettings::set_current_defaults(const sprite::Map& map) {
    std::unique_lock guard(defaults_mutex());
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    instance()->_current_defaults.set_print_by_default(false);
    instance()->_current_defaults = map;
}

/*sprite::Map& GlobalSettings::current_defaults_with_config() {
    std::unique_lock guard(defaults_mutex());
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    return instance()->_current_defaults_with_config;
}*/

void GlobalSettings::set_current_defaults_with_config(const sprite::Map& map) {
    std::unique_lock guard(defaults_mutex());
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    instance()->_current_defaults_with_config.set_print_by_default(false);
    instance()->_current_defaults_with_config = map;
}

/*const sprite::Map& GlobalSettings::defaults() {
    std::unique_lock guard(defaults_mutex());
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    return instance()->_config.defaults;
}

std::shared_ptr<const sprite::PropertyType> GlobalSettings::get_default(std::string_view name) {
    std::unique_lock guard(defaults_mutex());
    if (not instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    auto value = instance()->_config.get_default(name);
    if(value) {
        return value->ptr();
    }
    return nullptr;
}

void GlobalSettings::set_defaults(const sprite::Map& map) {
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    instance()->_config.defaults;
}

docs_map_t& GlobalSettings::docs() {
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    return instance()->_config.docs;
}

user_access_map_t& GlobalSettings::access_levels() {
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    return instance()->_config.access;
}*/

/**
 * @param name
 * @return sprite::Reference
 */
sprite::Reference GlobalSettings::_get(std::string_view name) {
    return write_value<NoType>(name);
}

AccessLevel GlobalSettings::access_level(std::string_view name) {
    return read([name](const Configuration& config) -> AccessLevel {
        auto it = config.access.find(name);
        if(it == config.access.end())
            return AccessLevelType::PUBLIC;
        return it->second;
    });
}

void GlobalSettings::set_access_level(const std::string& name, AccessLevel w) {
    write([&name, w](Configuration& config) {
        config.set_access(name, w);
    });
}

bool GlobalSettings::has_access(const std::string &name, AccessLevel level) {
    return level >= access_level(name);
}

/**
 * Loads parameters from a file.
 * @param filename Name of the file
 */
std::map<std::string, std::string> GlobalSettings::load_from_file(
    const std::string& filename,
    LoadOptions options
) {
    std::string fileContent = file::Path(filename).read_file();
    options.source = sprite::MapSource{filename};
    
    if(options.target) {
        return load_from_string(
            fileContent,
            options
        );
    }
    
    return write([options = std::move(options), fileContent](Configuration& config) mutable {
        options.target = &config.values;
        return load_from_string(
            fileContent,
            options
        );
    });
}

/**
 * Loads parameters from a string.
 * @param str the string
 */
std::map<std::string, std::string> GlobalSettings::load_from_string(
    const std::string& file,
    LoadOptions options
) {
    if(not options.target)
        throw InvalidArgumentException("load_from_string needs a target to write to.");
    
    const auto& deprecations = options.deprecations;
    AccessLevel access = options.access;
    bool correct_deprecations = options.correct_deprecations;
    const auto& exclude = options.exclude;
    const sprite::Map* additional = options.additional;
    sprite::Map& map = *options.target;
    
    /*struct G {
        std::string s;
        G(const std::string& name) : s(name) {
            DebugHeader("// LOADING FROM ", s);
        }
        ~G() {
            DebugHeader("// LOADED ", s);
        }
    } g(source.name);*/

    std::stringstream line;
    std::map<std::string, std::string> rejected;
    
    for (size_t i=0; i<=file.length(); i++) {
        auto c = i >= file.length() ? '\n' : file.at(i);
        if (c == '\n' ) {
            auto str = utils::trim(line.str());
            
            try {
                if (!str.empty() && utils::contains(str, "=") && !utils::beginsWith(str, '#')) {
                    auto parts = utils::split(str, '=');
                    if (parts.size() == 2) {
                        auto var = (std::string)utils::trim(parts.at(0));
                        auto val = (std::string)utils::trim(parts.at(1));
                        
                        if(access_level(var) <= access
                           && not contains(exclude, var))
                        {
                            if(correct_deprecations
                               && deprecations.correct_deprecation(var, val, additional, map))
                            {
                                /// nothing?
                            } else if(not correct_deprecations
                                      && deprecations.is_deprecated(var))
                            {
                                /// nothing
                            } else if(map.has(var)) {
                                auto& obj = map[var].get();
                                obj.set_value_from_string(val);
                            } else if(additional && additional->has(var)) {
                                additional->at(var).get().copy_to(map);
                                map[var].get().set_value_from_string(val);
                            } else {
                                sprite::parse_values(options.source, map,"{"+var+":"+val+"}", nullptr, {}, {});
                            }
                            
                            rejected.insert({var, val});
                        }
                    }
                }
            } catch(const std::exception& e) {
                if(GlobalSettings::is_runtime_quiet(&map))
                {
                    if(str.length() > 150) {
                        str = utils::ShortenText(str, 150);
                    }
                    Print("Line ", str," cannot be loaded. (",std::string(e.what()),")");
                }
            }
            
            line.str("");
            
        } else if(c != '\r') {
            line << c;
        }
    }
    
    return rejected;
}

std::pair<int,int> GlobalSettings::calculateEasterDate(int year) {
    int a = year % 19;
    int b = year / 100;
    int c = year % 100;
    int d = b / 4;
    int e = b % 4;
    int f = (b + 8) / 25;
    int g = (b - f + 1) / 3;
    int h = (19 * a + b - d - g + 15) % 30;
    int i = c / 4;
    int k = c % 4;
    int l = (32 + 2 * e + 2 * i - h - k) % 7;
    int m = (a + 11 * h + 22 * l) / 451;
    int monthE = (h + l - 7 * m + 114) / 31;
    int dayE = ((h + l - 7 * m + 114) % 31) + 1;
    return {monthE, dayE};
}

SeasonType::Class GlobalSettings::currentSeason() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm = *std::localtime(&t);
    int year = local_tm.tm_year + 1900;
    int month = local_tm.tm_mon + 1;
    int day = local_tm.tm_mday;
    auto [eMonth, eDay] = calculateEasterDate(year);
    // Compute day-of-year for current date and Easter date
    std::tm easterTm = {};
    easterTm.tm_year = local_tm.tm_year;
    easterTm.tm_mon = eMonth - 1;
    easterTm.tm_mday = eDay;
    std::mktime(&easterTm);
    int easterYday = easterTm.tm_yday + 1;
    int currentYday = local_tm.tm_yday + 1;
    // New Year's holiday: Dec 31 and Jan 1
    if ((month == 12 && day == 31) || (month == 1 && day == 1))
        return SeasonType::NEW_YEAR;
    // Easter holiday: one week before Good Friday through Easter Monday
    if (currentYday >= easterYday - 9 && currentYday <= easterYday + 1)
        return SeasonType::EASTER;

    // Star Wars Day: May 4
    if (month == 5 && day == 4)
        return SeasonType::STAR_WARS_DAY;

    // Pride Month: week before and month of June
    if ((month == 5 && day >= 25) || month == 6)
        return SeasonType::PRIDE_MONTH;

    // Christmas holiday: one week before Dec 24 through Dec 26
    if (month == 12 && day >= 17 && day <= 26)
        return SeasonType::CHRISTMAS;
    switch (month) {
        case 3: case 4: case 5:
            return SeasonType::SPRING;
        case 6: case 7: case 8:
            return SeasonType::SUMMER;
        case 9: case 10: case 11:
            return SeasonType::AUTUMN;
        default:
            return SeasonType::WINTER;
    }
}

}
