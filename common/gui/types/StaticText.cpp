#include "StaticText.h"
#include <gui/DrawSFBase.h>
#include <misc/GlobalSettings.h>
#include <misc/pretty.h>
#include <gui/Graph.h>

namespace cmn::utils {
using namespace cmn::gui;

using word_break_t = std::tuple<size_t, Float2_t, Float2_t>;
using word_break_cache_t = std::vector<word_break_t>;

Bounds calculate_bounds(std::string_view text, Drawable* reference, const Font& font) {
    return Base::default_text_bounds(text, reference, font);
}

inline bool is_whitespace(char c) {
    // Characters allowed when close to the end of the segment.
    return std::isspace(static_cast<unsigned char>(c));
    return c == ' ' ;
}

inline bool is_extended_whitespace(char c) {
    // Characters allowed when further away from the end.
    return is_in(c, '/', '\\', '-', '_', '.', ',', ':');
    //c == '/' || c == '\\' || c == ',' || c == '-' || c == '_';
    //c == ':' || || c == ' ' || c == ',' || c == '-' || c == ':' || c == ',' ||
           //c == '/' || c == '\\' || c == '.' || c == '_';
}

word_break_cache_t word_breaks_cache(std::string_view str, Drawable* reference, const Font& font) {
    word_break_cache_t result;
    bool in_whitespace{false};
    const size_t N = str.length();
    Float2_t total_width = 0;
    
    for(size_t i = 0; i < N; ++i) {
        if(const bool ws = is_whitespace(str[i]);
           ws || is_extended_whitespace(str[i]))
        {
            if(not in_whitespace) {
                if(i > 0) {
                    size_t previous = result.empty() ? 0 : std::get<0>(result.back());
                    size_t current = i;
                    
                    if(not ws)
                        ++current;
                    
                    Bounds bounds = calculate_bounds(str.substr(previous, current - previous), reference, font);
                    //Print(bounds, " => ",(std::string)str.substr(previous, i - previous));
                    total_width += bounds.width;
                    result.emplace_back(current, bounds.width, total_width);
                }
                in_whitespace = true;
            }
        } else if(in_whitespace) {
            in_whitespace = false;
        }
    }
    if(N > 0
       && (result.empty()
           || std::get<0>(result.back()) != N))
    {
        size_t previous = result.empty() ? 0 : std::get<0>(result.back());
        auto sub = str.substr(previous, N - previous);
        if(not sub.empty()) {
            Bounds bounds = calculate_bounds(sub, reference, font);
            total_width += bounds.width;
            //Print(bounds, " => ",(std::string)str.substr(previous, N - previous));
            result.emplace_back(N, bounds.width, total_width);
        }
    }
    return result;
}

size_t find_splitting_point(std::span<word_break_t> word_breaks,
                            std::string_view str, const Float2_t w,
                              const Float2_t max_w, Drawable* reference,
                            const Font& font);

size_t find_splitting_point(std::string_view str, const Float2_t w,
                              const Float2_t max_w, Drawable* reference,
                              const Font& font)
{
    if(w <= max_w)
        return str.length();
    auto cache = word_breaks_cache(str, reference, font);
    return find_splitting_point(cache, str, w, max_w, reference, font);
}

size_t find_splitting_point(std::span<word_break_t> word_breaks,
                            std::string_view str, const Float2_t w,
                              const Float2_t max_w, Drawable* reference,
                              const Font& font)
{
    std::optional<size_t> new_index;
    //size_t new_iterations = 1;
    
    if(not word_breaks.empty()
       && std::get<2>(word_breaks.front()) <= max_w)
    {
        size_t low = 0;
        size_t high = word_breaks.size() - 1;
        std::optional<size_t> index;
        
        while (low <= high) {
            const size_t middle = low + (high - low) / 2;
            auto tw = std::get<2>(word_breaks[middle]);
            //++new_iterations;
            if(tw == max_w) {
                index = middle;
                break;
            } else if(tw < max_w) {
                low = middle + 1;
            } else /*if(tw > max_w)*/ {
                high = middle - 1;
            }
        }
        
        if(not index) {
            auto ma = low, mi = high;
            if(high > low)
                std::swap(ma, mi);
            
            //++new_iterations;
            if(ma < word_breaks.size()
               && std::get<2>(word_breaks[ma]) <= max_w)
            {
                index = ma;
            } else {
                index = mi;
            }
        }
        
        new_index = std::get<0>(word_breaks[*index]);
        //Print("Would go for idx ", word_breaks[*index], " with max_w=", max_w);
    }
    
#ifndef NDEBUG
    Float2_t prev = 0;
    for(auto &[_, _, cum] : word_breaks) {
        assert(prev <= cum);
        prev = cum;
    }
#endif

    //Float2_t cw = w;
    /*size_t old_iterations = 0;
    auto it = word_breaks.rbegin();
    std::optional<size_t> idx;
    while (cw > max_w
           && it != word_breaks.rend())
    {
        auto total = std::get<2>(*it);
        cw -= std::get<1>(*it); // subtract current word + whitespaces
        ++it;
        ++old_iterations;
        if(it != word_breaks.rend()
           && cw <= max_w)
        {
            idx = std::get<0>(*it);
            //Print("Found cw=", cw, " vs ", std::get<2>(*it)," string would be ", (std::string)str.substr(0, *idx));
        } else if(it != word_breaks.rend()) {
            //Print("cw=",cw," > ", max_w);
        } else if(total <= max_w) {
            idx = std::get<0>(word_breaks.front());
            //Print("Reached the end of word_breaks, but ", total, " <= ", max_w,"?");
        } else {
            /// have to break within the first word?
            str = str.substr(0, std::get<0>(word_breaks.front()));
            cw = std::get<2>(word_breaks.front());
        }
    }
    
    static thread_local double samples = 0, nit = 0, oit = 0;
    ++samples;
    nit += new_iterations;
    oit += old_iterations;
    
    if(size_t(samples) % 100 == 0) {
        Print("* iterations Old(", oit / samples,") New(", nit / samples, ")");
    }
    
    if(idx != new_index) {
        Print("Old would go for idx ", idx, " vs ", new_index, " for max_w=",max_w,". Differences:");
        if(idx)
            Print(std::string(str.substr(0, *idx)),  " => ", calculate_bounds(str.substr(0, *idx), reference, font).width);
        else
            Print("null");
        if(new_index)
            Print(std::string(str.substr(0, *new_index)),  " => ", calculate_bounds(str.substr(0, *new_index), reference, font).width);
        else
            Print("null");
    }
    if(idx)
        return *idx;*/
    
    if(new_index)
        return *new_index;
    
    
    Float2_t cw = w;
    if(not word_breaks.empty()) {
        str = str.substr(0, std::get<0>(word_breaks.front()));
        cw = std::get<2>(word_breaks.front());
    }

    // If no acceptable break was found, try to subdivide the segment.
    //cw = w;

    size_t idx = 0;
    if(cw > max_w) {
        // we have to break it up.
        size_t len = str.length();
        size_t middle = len * 0.5;
        idx = middle;

        while (true) {
            if(len <= 1)
                break;

            Bounds bounds = calculate_bounds(
                 std::string_view(str.data(), middle),
                //StaticText::RichString::parse(std::string_view(str.data(), middle)),
                reference, font
            );
            cw = utils::calculate_width(bounds);

            if(cw <= max_w) {
                middle = middle + len * 0.25;
                len = len * 0.5;
            } else { // cw > max_w
                middle = middle - len * 0.25;
                len = len * 0.5;
            }
        }

        if (middle == 0 && not str.empty())
            idx = 1;
        else
            idx = middle;
    }
    else
        return str.length();
    // Otherwise, the entire segment fits on a new line.

    return idx;
}

}

namespace cmn::gui {
    static bool nowindow_updated = false;
    static bool nowindow;
    
TRange::TRange(std::string_view n, size_t i, size_t before)
    : range(i, i), name(n), after(0), before(before)
{
}

void TRange::close(size_t i, const std::string_view& text, size_t after) {
    range.end = i;
    this->text = text.substr(range.start, range.end - range.start);
    this->after = after;
}

bool TRange::operator<(const TRange& other) const {
    return range < other.range;
}

std::string TRange::toStr() const {
    return "TRange<"+std::string(name)+"> "+Meta::toStr(range)+" "+Meta::toStr(subranges)+" '"+std::string(text)+"'";
}

void StaticText::set_txt(const std::string& txt) {
    auto p = utils::find_replace(txt, "<br/>", "\n");
    
    if(_settings.txt == p)
        return;
    
    _settings.txt = p;
    update_text();
}
    
    Size2 StaticText::size() {
        update();
        return Entangled::size();
    }
    
    const Bounds& StaticText::bounds() {
        update();
        return Entangled::bounds();
    }
    
    void StaticText::structure_changed(bool downwards) {
        Entangled::structure_changed(downwards);
    }
    
    void StaticText::set_size(const Size2& size) {
        if(not bounds().size().Equals(size)) {
            if(_origin != Vec2(0))
                structure_changed(true);
            set_content_changed(true);
        }
        Entangled::set_size(size);
    }

void StaticText::set_default_font(Font font) {
    font.align = Align::Left;
    if(font == _settings.default_font)
        return;
    
    _settings.default_font = font;
    set_content_changed(true);
    update_text();
}
            
    void StaticText::set_max_size(const Size2 &size) {
        if(size != _settings.max_size) {
            _settings.max_size = size;
            set_content_changed(true);
            update_text();
        }
    }
    
    void StaticText::set_margins(const Margins &margin) {
        if(_settings.margins == margin)
            return;
        
        _settings.margins = margin;
        set_content_changed(true);
        update_text();
    }

#define yet
    
    void StaticText::update() {
        if(_content_changed && not _begun /* && not _content_changed_while_updating*/) {
            _content_changed = false;
            
            auto ctx = OpenContext();
            
            // find enclosing rectangle dimensions
            Vec2 m(0);
            //bool hiding_something{false};
            
            for(auto& t : texts) {
                if(not t || t->txt().empty())
                    continue;
                
                // add texts so that dimensions are retrieved
                t->set_color(t->color().alpha(255 * _settings.alpha));
                //t->set_color(t->color().alpha((float(t->color().a) / 255.f) * _settings.alpha * 255));
                t->set(Text::Shadow_t{_settings.shadow});
                
                if(_settings.max_size.y > 0
                   && t->pos().y + t->size().height * (1 - t->origin().y) > _settings.max_size.y)
                {
#ifndef NDEBUG
                   // Print("Out of bounds: ", t->pos().y, " + ", t->size().height , " > ", _settings.max_size.y, ": ", t->txt());
#endif
                    //hiding_something = true;
                    break;
                    
                } else {
                    advance_wrap(*t);
                }
                
                auto local_pos = t->pos() - t->size().mul(t->origin());
                auto v = local_pos + t->size(); //+ t->text_bounds().pos();
                
                m.x = max(m.x, v.x);
                m.y = max(m.y, v.y);
            }
            
            // subtract position, add margins
            if(not m.empty()) {
                m = m + _settings.margins.size();
                if(_settings.max_size.y > 0) {
                    m.y = min(m.y, _settings.max_size.y);
                }
            }
            
            //Print("Setting size of ", _settings.txt, " to ", m);
            set_size(m);
            
            if(_settings.fade_out > 0) {
                add_shadow();
                
            } //else
                //_fade_out = nullptr;
            
            if(bg().fill) {
                Entangled::set(FillClr{bg().fill.value().alpha(_settings.alpha * _settings.fill_alpha * 255)});
            }
            if(bg().line) {
                Entangled::set(LineClr{bg().line.value().alpha(_settings.alpha * _settings.fill_alpha * 255)});
            }
            
            /*if(bg_fill_color() != Transparent || bg_line_color() != Transparent)
                set_background(bg_fill_color() != Transparent
                               ? bg_fill_color().alpha(_settings.alpha * _settings.fill_alpha * 255)
                               : Transparent,
                    bg_line_color() != Transparent
                               ? bg_line_color().alpha(_settings.alpha * _settings.fill_alpha * 255)
                               : Transparent);*/
            
            set_content_changed(false);
        }
    }

void StaticText::set(FillClr fill) {
    _settings.fill_alpha = double(fill.a) / 255.0;
    Entangled::set(fill);
}

/*void StaticText::set_background(const Color& color, const Color& line) {
    _settings.fill_alpha = double(color.a) / 255.0;
    Entangled::set_background(color, line);
}*/

StaticText::RichString::RichString(std::string_view str, const Font& font, const Vec2& pos, const Color& clr)
    : str(str), font(font), pos(pos), clr(clr)
{
    //parsed = parse(str);
}

std::string StaticText::RichString::parse(std::string_view txt) {
    return utils::find_replace(txt, {
        {"&quot;", "\""},
        {"&apos;", "'"},
        {"&lt;", "<"},
        {"&gt;", ">"},
        { "&#x3C;", "<"}
    });
}

void StaticText::RichString::convert(const std::unique_ptr<Text>& text) const {
    text->set_color(clr);
    text->set_font(font);
    if(text->txt() != str) {
        text->set_txt(std::string(str));
    }
}

void StaticText::add_shadow() {
    if(not _fade_out)
        _fade_out = new ExternalImage();
    
    Float2_t h = min(height(), Base::default_line_spacing(_settings.default_font) * 3_F);
    auto image = Image::Make(height()+1, 1, 4);
    image->set_to(0);
    
    auto bg = bg_fill_color();
    auto ptr = parent();
    while(bg.a == 0 && ptr) {
        bg = ptr->bg_fill_color();
        ptr = ptr->parent();
    }
    image->get().setTo(cv::Scalar(bg.alpha(0)));
    
    const Float2_t start_y = height() - h;
    const Float2_t end_y = height() + 1;
    
    for(uint y=start_y; y<end_y; ++y) {
        auto percent = saturate(Float2_t(y-start_y) / Float2_t(end_y - start_y + 1_F), 0_F, 1_F);
        percent *= percent;
        //percent = 1;
        auto ptr = image->ptr(y, 0);
        //*(ptr+0) = saturate((percent) * 255, 0, 255);
        //*(ptr+1) = saturate((percent) * 255, 0, 255);
        //*(ptr+2) = saturate((percent) * 255, 0, 255);
        *(ptr+3) = saturate((percent) * _settings.fade_out * 255, 0, 255);
    }
    
    _fade_out->set_source(std::move(image));
    _fade_out->set(Loc{0, 0});
    _fade_out->set(Scale{width(),1});
    advance_wrap(*_fade_out);
}

void StaticText::add_string(
        Drawable* reference,
        const Settings& _settings,
        RichString&& ptr,
        std::vector<RichString>& strings,
        Vec2& offset)
{
    if (_settings.max_size.x < 1
        || ptr.str.empty()
        || std::isnan(_settings.max_size.x))
    {
        strings.push_back(std::move(ptr));
        return;
    }
    
    const Bounds bounds = utils::calculate_bounds(ptr.str, reference, ptr.font);
    auto w = utils::calculate_width(bounds);
    
    /// in case we need to add a next line
    std::optional<RichString> add_after = std::move(ptr);
    std::optional<utils::word_break_cache_t> org_cache;
    std::optional<std::span<utils::word_break_t>> cache;
    
    const Float2_t _max_w = _settings.max_size.x - _settings.margins.x - _settings.margins.x;
    //Print("** ", utils::ShortenText(ptr.str, 15)," font=",ptr.font);
    while(add_after) {
        auto ptr = add_after.value();
        add_after.reset();
        
        const Float2_t max_w = _max_w - offset.x;
        
        if(w <= max_w) {
            offset.x += w;
            strings.push_back(std::move(ptr));
            break;
        }
        
        if(not org_cache) {
            org_cache = utils::word_breaks_cache(ptr.str, reference, ptr.font);
            cache = std::span(*org_cache);
        }
            
        size_t idx = utils::find_splitting_point(*cache, ptr.str, w, max_w, reference, ptr.font);
        
        offset.y++;
        offset.x = 0;

        if (idx) {
            if(ptr.str.length() > idx) {
                auto sub = ptr.str.substr(idx);
                auto trimmed = utils::ltrim(sub);
                size_t trim_d = sub.length() - trimmed.length();
                if(trimmed.length() > 0) {
                    // if there is some remaining non-whitespace
                    // string, add it recursively
                    RichString tmp;
                    tmp.str = trimmed;
                    tmp.font = ptr.font;
                    tmp.pos.x = ptr.pos.x;
                    tmp.pos.y = ptr.pos.y + 1;
                    tmp.clr = ptr.clr;
                    
#ifndef NDEBUG
                    assert(cache.has_value());
                    Print("Cache: ", *cache);
                    auto alt_cache = utils::word_breaks_cache(tmp.str, reference, tmp.font);
                    Print("Expected: ", alt_cache);
#endif
                    
                    size_t d = 0;
                    size_t span_offset = 0;
                    Float2_t removed_w = 0;//trim_d > 0 ? utils::calculate_bounds(sub.substr(0, trim_d), reference, ptr.font).width : 0;
                    for(auto it = cache.value().begin(); it != cache.value().end(); ) {
                        if(std::get<0>(*it) <= idx + trim_d) {
                            d = idx + trim_d;
                            removed_w += std::get<1>(*it);
                            //it = cache.value().erase(it);
                            w -= std::get<1>(*it);
                            ++span_offset;
                            ++it;
                        } else {
                            std::get<0>(*it) -= d;
                            std::get<2>(*it) -= removed_w;
                            ++it;
                        }
                    }
                    
                    cache = std::span(cache.value().begin() + span_offset, cache.value().end());
                    
                    /// we did not find anything beyond the first word
                    /// so we will have to cut into it (remove part of it).
                    /// this is most likely because we only have one
                    if(idx > 0
                       && d == 0)
                    {
                        if(not cache->empty()) {
                            for(auto &f : *cache)
                                std::get<0>(f) -= idx + trim_d;
                            
                            auto& f = cache->front();
                            auto p = std::get<1>(f);
                            std::get<1>(f) = utils::calculate_bounds(tmp.str.substr(0, std::get<0>(f)), reference, ptr.font).width;
                            
                            auto diff = p - std::get<1>(f);
                            w -= diff;
                            
                            for(auto &f : *cache)
                                std::get<2>(f) -= diff;
                        }
                    }
                    
                    /// we trimmed some whitespace off, so we need to subtract from all the widths
                    /// that have been cached:
                    if(trim_d > 0
                       && not cache->empty())
                    {
                        auto& f = cache->front();
                        auto p = std::get<1>(f);
                        std::get<1>(f) = utils::calculate_bounds(tmp.str.substr(0, std::get<0>(f)), reference, ptr.font).width;
                        auto diff = p - std::get<1>(f);
                        w -= diff;
                        
                        for(auto &f : *cache)
                            std::get<2>(f) -= diff;
                    }
                    
#ifndef NDEBUG
                    if(not std::ranges::equal(alt_cache, *cache)) {
                        Print("DIfference: ", *cache);
                    }
                    if(auto new_w = utils::calculate_bounds(tmp.str, reference, ptr.font).width;
                       abs(w - new_w) >= 1)
                    {
                        Print("Width difference: ", w, " != ", new_w);
                    }
#endif
                    add_after = std::move(tmp);
                }
            }
            
            /// shorten the current line segment
            ptr.str = ptr.str.substr(0, idx);
            strings.push_back(std::move(ptr));
            
            continue;

        } else {
            // put the whole text in the next line
            ptr.pos.y++;
        }
        
        offset.x += w;
        strings.push_back(std::move(ptr));
    }
}

std::vector<TRange> StaticText::to_tranges(const std::string& _txt) {
    char quote = 0;
    
    std::deque<TRange> tags;
    std::vector<TRange> global_tags;
    
    size_t before_pos = 0;
    int64_t brackets{0};
    
    static constexpr std::array<std::string_view, 25> commands {
        "h","h1","h2","h3","h4","h5","h6","h7","h8","h9", "i","c","b","string","number","str","nr","keyword","key","ref","a","sym","orange","cyan","small"
    };
    
    for(size_t i=0, N = _txt.size(); i<N; ++i) {
        const char& c = _txt[i];
        
        if(c == '\'' ||c == '"') {
            if(quote == c)
                quote = 0;
            else
                quote = c;
            
        } else if(/*!quote &&*/ c == '<') {
            if(brackets == 0)
                before_pos = i;
            ++brackets;
            
        } else if(/*!quote &&*/ c == '>') {
            std::string_view s;
            if(brackets > 0
               && i >= before_pos + 1
               && _txt.length() > before_pos + 1)
            {
                --brackets;
                
                s = std::string_view(_txt.c_str() + before_pos + 1,
                                     i - before_pos - 1);
            }
            
            if(!s.empty()) {
                if(s[0] == '/') {
                    // ending tag
                    if(!tags.empty() && tags.front().name == s.substr(1)) {
                        auto front = std::move(tags.front());
                        front.close(before_pos, _txt, i+1);
                        
                        tags.pop_front();
                        if(tags.empty()) {
                            global_tags.emplace_back(std::move(front));
                        } else {
                            tags.front().subranges.emplace(std::move(front));
                        }
                        
                    }
#ifndef NDEBUG
                    else
                        Print("Cannot pop tag ",s);
#endif
                } else {
                    //std::string l = utils::lowercase(s);
                    //Print("command: ", utils::lowercase(s));
                    //Print("all = ", AllColors);
                    if(not contains(commands, s)
                       && not contains(AllColors, s))
                    {
                        if(tags.empty()) {
                            global_tags.push_back(TRange("_", global_tags.empty() ? 0 : global_tags.back().after, global_tags.empty() ? 0 : global_tags.back().range.end));
                            global_tags.back().close(i+1, _txt, i+1);
                        }
                    } else {
                        if(tags.empty()) {
                            if((global_tags.empty() && before_pos > 0) || !global_tags.empty()) {
                                global_tags.push_back(TRange("_", global_tags.empty() ? 0 : global_tags.back().after, global_tags.empty() ? 0 : global_tags.back().range.end));
                                global_tags.back().close(before_pos, _txt, i+1);
                            }
                        }
                        
                        tags.push_front(TRange(s, i + 1, before_pos));
                    }
                }
            }
        
            before_pos = i+1;
            
        }
    }
    
    if(!tags.empty()) {
        auto front = tags.front();
        tags.pop_front();
        
        front.close(_txt.size(), _txt, _txt.size());
        global_tags.push_back(front);
        if(!tags.empty())
            FormatWarning("Did not properly close all tags.");
    } else if(global_tags.empty() || global_tags.back().after < _txt.size()) {
        global_tags.push_back(TRange("_", global_tags.empty() ? 0 : global_tags.back().after, global_tags.empty() ? 0 : global_tags.back().range.end));
        global_tags.back().close(_txt.size(), _txt, _txt.size());
    }
    
    return global_tags;
}
    
template<typename T, typename K = T, typename V, class... D, template <typename, class...> class PtrType>
    requires (not std::is_pointer_v<V> && not _is_smart_pointer<V>)
inline void update_vector_elements(
   std::vector<PtrType<T, D...>>& vector,
   const std::vector<V>& compare,
   const std::function<void(const PtrType<T, D...>&,
   const V&)>& prepare = nullptr)
{
    // Resize vector to match compare, default-constructing new elements as needed
    if (vector.size() > compare.size()) {
        vector.resize(compare.size());
    }

    // Prepare and convert existing elements
    for (size_t i = 0; i < vector.size(); ++i) {
        if (prepare) {
            prepare(vector[i], compare[i]);
        }
        compare[i].convert(vector[i]);
    }

    // Add and prepare missing elements
    for (size_t i = vector.size(); i < compare.size(); ++i) {
        PtrType<T, D...> ptr = std::make_unique<K>();
        vector.emplace_back(std::move(ptr));

        if (prepare) {
            prepare(vector.back(), compare[i]);
        }
        compare[i].convert(vector.back());
    }
}

    void StaticText::update_text() {
        if(!nowindow_updated) {
            nowindow_updated = true;
            nowindow = BOOL_SETTING(nowindow);
        }
        
        const auto default_clr = _settings.text_color;
        static const auto highlight_clr = LightGray;
        
        //_txt = "a <b>very</b> long text, ja ja i <i>dont know</i> whats <b><i>happening</i></b>, my friend. <b>purple rainbows</b> keep bugging mees!\nthisisaverylongtextthatprobablyneedstobesplitwithouthavinganopportunitytoseparateitsomewhere<a custom tag>with text after";
        //_txt = "<a custom tag>";
        
        //_txt = "<h3>output_posture_data</h3>type: <keyword>bool</keyword>\ndefault: <keyword>false</keyword>\n\nSave posture data npz file along with the usual NPZ/CSV files containing positions and such. If set to <keyword>true</keyword>, a file called <string>'<ref>output_dir</ref>/<ref>fish_data_dir</ref>/<ref>filename</ref>_posture_fishXXX.npz'</string> will be created for each fish XXX.";
        
        // parse lines individually
        Vec2 offset(0, 0);
        
        _rich_strings.clear();
        _string_owner.clear();
        
        auto global_tags = to_tranges(text());
        
        auto mix_colors = [&](const Color& A, const Color& B) {
            //if(A != default_clr)
            return Color::blend(B.alpha(0.75 * 255), A.alpha(0.25 * 255)).alpha(saturate(A.a * 0.25 + B.a * 0.75, 0, 255));
            return Color::blend(B.alpha(0.75 * 255), A.alpha(0.25 * 255));
            //else
            //    return B;
        };
        
        std::deque<TRange> queue;
        for(auto && tag : global_tags) {
            tag.color = default_clr;
            tag.font = _settings.default_font;
            queue.emplace_back(std::move(tag));
        }
        
        while(!queue.empty()) {
            auto tag = std::move(queue.front());
            queue.pop_front();
            
            bool breaks_line = false;
            
            if(tag.name == "_");
                // default (global / empty style)
            else if(tag.name == "b")
                tag.font.style |= Style::Bold;
            else if(tag.name == "i")
                tag.font.style |= Style::Italic;
            else if(tag.name == "c")
                tag.font.style = Style::Monospace;
            else if(tag.name == "sym")
                tag.font.style = Style::Symbols;
            else if(tag.name == "key" || tag.name == "keyword") {
                tag.color = mix_colors(tag.color, Color(232, 85, 232, 255));
            }
            else if(tag.name == "str" || tag.name == "string") {
                tag.color = mix_colors(tag.color, Red);
            }
            else if(tag.name == "orange") {
                tag.color = mix_colors(tag.color, Orange);
            }
            else if(tag.name == "cyan") {
                tag.color = mix_colors(tag.color, Cyan);
            }
            else if(tag.name == "nr" || tag.name == "number") {
                tag.color = mix_colors(tag.color, Cyan);
            }
            else if(tag.name == "a") {
                tag.color = mix_colors(tag.color, Cyan);
            }
            else if(tag.name[0] == 'h') {
                if((tag.name.length() == 2 && tag.name[1] >= '0' && tag.name[1] < '9')
                   || tag.name == "h")
                {
                    tag.font.size = _settings.default_font.size * (1 + (1 - saturate(Float2_t(tag.name[1] - '0') / 6_F, 0_F, 1_F)));
                    tag.font.style |= Style::Bold;
                    tag.color = mix_colors(tag.color, highlight_clr);
                    //breaks_line = true;
                }
            }
            else if(tag.name == "small") {
                tag.font.size = _settings.default_font.size * 0.75;
            }
            else if(tag.name == "ref") {
                tag.font.style |= Style::Bold;
                tag.color = mix_colors(tag.color, Gray);
            }
            else {
                bool found = false;
                for(auto &c : AllColors) {
                    if(utils::lowercase(tag.name) == utils::lowercase(c.name)) {
                        tag.color = mix_colors(tag.color, c.color);
                        found = true;
                        break;
                    }
                }
                
                if(not found)
                    Print("Unknown tag ",tag.name," in RichText.");
            }
            
            if(!tag.subranges.empty()) {
                assert(tag.text.data() >= _settings.txt.c_str()
                       && tag.text.data() + tag.text.length() <= _settings.txt.c_str() + _settings.txt.length());
                
                auto sub = *tag.subranges.begin();
                tag.subranges.erase(tag.subranges.begin());
                
                assert(sub.text.data() >= _settings.txt.c_str()
                       && sub.text.data() + sub.text.length() <= _settings.txt.c_str() + _settings.txt.length());
                
                assert(tag.text.length() == tag.range.length());
                
                auto bt = tag.text.substr(0, sub.before - tag.range.start);
                auto array = utils::split(bt, '\n');
                for(size_t k=0; k<array.size(); ++k) {
                    if(array[k].data() != nullptr) {
                        assert(array[k].data() >= _settings.txt.c_str()
                               && array[k].data() + array[k].length() <= _settings.txt.c_str() + _settings.txt.length());
                    }
                    
                    if(k > 0) {
                        ++offset.y;
                        offset.x = 0;
                    }
                    
                    _string_owner.push_back(std::make_unique<std::string>(RichString::parse(array[k])));
                    add_string(this, _settings, RichString{
                        std::string_view(*_string_owner.back()),
                        tag.font,
                        offset,
                        tag.color
                    }, _rich_strings, offset);
                }
                
                tag.text = tag.text.substr(sub.after - tag.range.start);
                tag.range.start = sub.after;
                
                sub.font = tag.font;
                sub.color = tag.color;
                
                assert(tag.text.length() == tag.range.length());
                
                assert(tag.text.data() >= _settings.txt.c_str()
                       && tag.text.data() + tag.text.length() <= _settings.txt.c_str() + _settings.txt.length());
                
                queue.emplace_front(std::move(tag));
                queue.emplace_front(std::move(sub));
            } else {
                assert(tag.text.data() >= _settings.txt.c_str()
                       && tag.text.data() + tag.text.length() <= _settings.txt.c_str() + _settings.txt.length());
                auto array = utils::split(tag.text, '\n');
                for(size_t k=0; k<array.size(); ++k) {
                    if(array[k].data() != nullptr) {
                        assert(array[k].data() >= _settings.txt.c_str()
                               && array[k].data() + array[k].length() <= _settings.txt.c_str() + _settings.txt.length());
                    }
                    
                    if(k > 0) {
                        ++offset.y;
                        offset.x = 0;
                    }
                    
                    _string_owner.push_back(std::make_unique<std::string>(RichString::parse(array[k])));
                    add_string(this, _settings, RichString{
                        std::string_view(*_string_owner.back()),
                        tag.font,
                        offset,
                        tag.color
                    }, _rich_strings, offset);
                }
                if(breaks_line) {
                    ++offset.y;
                    offset.x = 0;
                }
            }
        }
        
        update_vector_elements(texts, _rich_strings);
        
        offset = _settings.margins.pos();
        Float2_t y = 0;
        //float height = Base::default_line_spacing(_default_font);
        Font prev_font = _rich_strings.empty()
            ? _settings.default_font
            : _rich_strings.front().font;
        
        //begin();
        
        size_t row_start = 0;
        for(size_t i=0; i<texts.size(); i++) {
            auto &text = texts[i];
            auto &s = _rich_strings[i];
            
            text->set_origin(Vec2(0, 0.5));
            
            if(i >= positions.size())
                positions.push_back(Vec2());
            auto target = positions[i];
            
            if(s.pos.y > y) {
                offset.x = _settings.margins.x;
                
                auto current_height = (Base::default_line_spacing(prev_font) * 0.5);
                for(size_t j=row_start; j<i; ++j) {
                    texts[j]->set_pos(Vec2(texts[j]->pos().x, offset.y + current_height));
                }
                
                offset.y += Base::default_line_spacing(prev_font);//sf_line_spacing(s->font);
                y++;
                
                prev_font = s.font;
                row_start = i;
                
            } else if(s.font.size > prev_font.size)
                prev_font = s.font;
            
            target = Vec2(offset.x, offset.y);
            text->set_pos(target);
            //advance_wrap(*text);
            
            offset.x += (text->text_bounds().width + text->text_bounds().x);
        }
        
        auto current_height = Base::default_line_spacing(prev_font);
        for(size_t j=row_start; j<texts.size(); ++j) {
            texts[j]->set_pos(Vec2(texts[j]->pos().x, offset.y + current_height * 0.5_F));
        }
        
        //end();
        
        set_content_changed(true);
    }
}
