#include "StaticText.h"
#include <gui/DrawSFBase.h>
#include <misc/GlobalSettings.h>
#include <misc/pretty.h>
#include <gui/Graph.h>

namespace gui {
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
    
std::string TRange::class_name() {
    return "TRange";
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
            
            begin();
            
            // find enclosing rectangle dimensions
            Vec2 m(0);
            //bool hiding_something{false};
            
            for(auto& t : texts) {
                if(t->txt().empty())
                    continue;
                
                // add texts so that dimensions are retrieved
                t->set_color(t->color().alpha(255 * _settings.alpha));
                t->set(Text::Shadow_t{_settings.shadow});
                
                if(_settings.max_size.y > 0
                   && t->pos().y + t->size().height * (1 - t->origin().y) > _settings.max_size.y)
                {
#ifndef NDEBUG
                    print("Out of bounds: ", t->pos().y, " + ", t->size().height , " > ", _settings.max_size.y);
#endif
                    //hiding_something = true;
                    
                } else {
                    advance_wrap(*t);
                }
                
                auto local_pos = t->pos() - t->size().mul(t->origin());
                auto v = local_pos + t->size(); //+ t->text_bounds().pos();
                
                m.x = max(m.x, v.x);
                m.y = max(m.y, v.y);
            }
            
            // subtract position, add margins
            m = m + _settings.margins.size();
            if(_settings.max_size.y > 0) {
                m.y = min(m.y, _settings.max_size.y);
            }
            set_size(m);
            
            if(_settings.fade_out > 0) {
                add_shadow();
                
            } else
                _fade_out = nullptr;
            
            if(bg_fill_color() != Transparent || bg_line_color() != Transparent)
                set_background(bg_fill_color() != Transparent
                               ? bg_fill_color().alpha(_settings.alpha * _settings.fill_alpha * 255)
                               : Transparent,
                    bg_line_color() != Transparent
                               ? bg_line_color().alpha(_settings.alpha * _settings.fill_alpha * 255)
                               : Transparent);
            
            end();

            set_content_changed(false);
        }
    }

void StaticText::set_background(const Color& color, const Color& line) {
    _settings.fill_alpha = double(color.a) / 255.0;
    Entangled::set_background(color, line);
}

StaticText::RichString::RichString(const std::string& str, const Font& font, const Vec2& pos, const Color& clr)
    : str(str), font(font), pos(pos), clr(clr)
{
    parsed = parse(str);
}

std::string StaticText::RichString::parse(const std::string_view &txt) {
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
    text->set_txt(parsed);
}

void StaticText::add_shadow() {
    if(not _fade_out)
        _fade_out = std::make_shared<ExternalImage>();
    
    float h = min(height(), Base::default_line_spacing(_settings.default_font) * 3.f);
    auto image = Image::Make(height()+1, 1, 4);
    image->set_to(0);
    
    auto bg = _bg_fill_color;
    auto ptr = parent();
    while(bg.a == 0 && ptr) {
        bg = ptr->bg_fill_color();
        ptr = ptr->parent();
    }
    image->get().setTo(cv::Scalar(bg.alpha(0)));
    
    const float start_y = height() - h;
    const float end_y = height() + 1;
    
    for(uint y=start_y; y<end_y; ++y) {
        float percent = saturate(float(y-start_y) / float(end_y - start_y + 1), 0.f, 1.f);
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

    void StaticText::add_string(std::unique_ptr<RichString>&& ptr, std::vector<std::unique_ptr<RichString>> &strings, Vec2& offset)
    {
        
        /*if(_max_size.y > 0) {
            if(ptr->pos.y * Base::default_line_spacing(_default_font) >= _max_size.y ) {
                print("Cutting off ", ptr->str, "  at ", ptr->pos, " with max size ", _max_size);
                return;
            }
        }*/
        //const Vec2 stage_scale = this->stage_scale();
        //const Vec2 real_scale(1); //= this->real_scale();
        auto real_scale = this;
        
        if(_settings.max_size.x > 0 && !ptr->str.empty()) {
            Bounds bounds = Base::default_text_bounds(ptr->parsed, real_scale, ptr->font);
            auto w = bounds.width + bounds.x;
            
            const float max_w = _settings.max_size.x - _settings.margins.x - _settings.margins.x - offset.x;
            
            if(w > max_w) {
                float cw = w;
                size_t L = ptr->str.length();
                size_t idx = L;
                
                static const std::set<char> whitespace {
                    ' ',':',',','/','\\'
                };
                static const std::set<char> extended_whitespace {
                    ' ','-',':',',','/','\\','.','_'
                };
                
                while(cw > max_w && idx > 1) {
                    L = idx;
                    
                    // try to find a good splitting-point
                    // (= dont break inside words)
                    do --idx;
                    while(idx
                          && ((L-idx <= 10 && whitespace.find(ptr->str[idx-1]) == whitespace.end())
                           || (L-idx > 10  && extended_whitespace.find(ptr->str[idx-1]) == extended_whitespace.end())));
                          /*&& ptr->str[idx-1] != ' '
                          && ptr->str[idx-1] != '-'
                          && ptr->str[idx-1] != ':'
                          && ptr->str[idx-1] != ','
                          && ptr->str[idx-1] != '/'
                          && ptr->str[idx-1] != '.'
                          && ptr->str[idx-1] != '_');*/
                    
                    // didnt find a proper position for breaking
                    if(!idx)
                        break;
                    
                    // test splitting at idx
                    bounds = Base::default_text_bounds(RichString::parse(ptr->str.substr(0, idx)), real_scale, ptr->font);
                    
                    cw = bounds.width + bounds.x;
                }
                
                if(!idx) {
                    // can we put the whole segment in a new line, or
                    // do we have to break it up?
                    // do a quick-search for the best-fitting size.
                    cw = w;
                    
                    if(cw > _settings.max_size.x - _settings.margins.x - _settings.margins.x) {
                        // we have to break it up.
                        size_t len = ptr->str.length();
                        size_t middle = len * 0.5;
                        idx = middle;
                        
                        while (true) {
                            if(len <= 1)
                                break;
                            
                            bounds = Base::default_text_bounds(RichString::parse(ptr->str.substr(0, middle)), real_scale, ptr->font);
                            
                            cw = bounds.width + bounds.x;
                            
                            if(cw <= max_w) {
                                middle = middle + len * 0.25;
                                len = len * 0.5;
                                
                            } else if(cw > max_w) {
                                middle = middle - len * 0.25;
                                len = len * 0.5;
                            }
                        }
                        
                        idx = middle;
                        if(!idx && ptr->str.length() > 0)
                            idx = 1;
                        
                    } else {
                        // next line!
                    }
                }
                
                offset.y ++;
                offset.x = 0;
                
                if(idx) {
                    auto& obj = *ptr;
                    strings.emplace_back(std::move(ptr));
                    
                    std::string copy = obj.str;
                    obj.str = copy.substr(0, idx);
                    obj.parsed = RichString::parse(obj.str);
                    
                    copy = utils::ltrim(copy.substr(idx));
                    
                    // if there is some remaining non-whitespace
                    // string, add it recursively
                    if(!copy.empty()) {
                        auto tmp = std::make_unique<RichString>();
                        tmp->str = std::move(copy);
                        tmp->font = obj.font;
                        tmp->parsed = RichString::parse(tmp->str);
                        tmp->pos.x = obj.pos.x;
                        tmp->pos.y = obj.pos.y + 1;
                        tmp->clr = obj.clr;
                        add_string(std::move(tmp), strings, offset);
                    }
                    
                    return;
                    
                } else
                    // put the whole text in the next line
                    ptr->pos.y++;
            }
            
            offset.x += w;
        }
        
        strings.emplace_back(std::move(ptr));
    }

std::vector<TRange> StaticText::to_tranges(const std::string& _txt) {
    char quote = 0;
    
    std::deque<TRange> tags;
    std::vector<TRange> global_tags;
    
    size_t before_pos = 0;
    int64_t brackets{0};
    
    static constexpr std::array<std::string_view, 23> commands {
        "h","h1","h2","h3","h4","h5","h6","h7","h8","h9", "i","c","b","string","number","str","nr","keyword","key","ref","a","sym","orange"
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
            if(brackets > 0)
                --brackets;
            
            auto s = std::string_view(_txt.c_str() + before_pos + 1, i - before_pos - 1);
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
                        print("Cannot pop tag ",s);
#endif
                } else {
                    //std::string l = utils::lowercase(s);
                    if(not contains(commands, s)) {
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
    
    void StaticText::update_text() {
        if(!nowindow_updated) {
            nowindow_updated = true;
            nowindow = GlobalSettings::map().has("nowindow") ? SETTING(nowindow).value<bool>() : false;
        }
        
        const auto default_clr = _settings.text_color;
        static const auto highlight_clr = DarkCyan;
        
        //_txt = "a <b>very</b> long text, ja ja i <i>dont know</i> whats <b><i>happening</i></b>, my friend. <b>purple rainbows</b> keep bugging mees!\nthisisaverylongtextthatprobablyneedstobesplitwithouthavinganopportunitytoseparateitsomewhere<a custom tag>with text after";
        //_txt = "<a custom tag>";
        
        //_txt = "<h3>output_posture_data</h3>type: <keyword>bool</keyword>\ndefault: <keyword>false</keyword>\n\nSave posture data npz file along with the usual NPZ/CSV files containing positions and such. If set to <keyword>true</keyword>, a file called <string>'<ref>output_dir</ref>/<ref>fish_data_dir</ref>/<ref>filename</ref>_posture_fishXXX.npz'</string> will be created for each fish XXX.";
        
        // parse lines individually
        Vec2 offset(0, 0);
        
        std::vector<std::unique_ptr<RichString>> strings;
        
        auto global_tags = to_tranges(text());
        
        auto mix_colors = [&](const Color& A, const Color& B) {
            //if(A != default_clr)
            return Color::blend(B.alpha(0.75 * 255), A.alpha(0.25 * 255));
                return B * 0.75 + A * 0.25;
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
            else if(tag.name == "nr" || tag.name == "number") {
                tag.color = mix_colors(tag.color, Green);
            }
            else if(tag.name == "a") {
                tag.color = mix_colors(tag.color, Cyan);
            }
            else if(tag.name[0] == 'h') {
                if((tag.name.length() == 2 && tag.name[1] >= '0' && tag.name[1] < '9')
                   || tag.name == "h")
                {
                    tag.font.size = _settings.default_font.size * (1 + (1 - min(1, (tag.name[1] - '0') / 4.f)));
                    tag.font.style |= Style::Bold;
                    tag.color = mix_colors(tag.color, highlight_clr);
                    breaks_line = true;
                }
            }
            else if(tag.name == "ref") {
                tag.font.style |= Style::Bold;
                tag.color = mix_colors(tag.color, Gray);
            }
            else print("Unknown tag ",tag.name," in RichText.");
            
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
                    add_string(std::make_unique<RichString>( (std::string)array[k], tag.font, offset, tag.color ), strings, offset);
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
                    add_string(std::make_unique<RichString>( (std::string)array[k], tag.font, offset, tag.color ), strings, offset);
                }
                if(breaks_line) {
                    ++offset.y;
                    offset.x = 0;
                }
            }
        }
        
        update_vector_elements(texts, strings);
        
        offset = _settings.margins.pos();
        float y = 0;
        //float height = Base::default_line_spacing(_default_font);
        Font prev_font = strings.empty() ? _settings.default_font : strings.front()->font;
        
        //begin();
        
        size_t row_start = 0;
        for(size_t i=0; i<texts.size(); i++) {
            auto &text = texts[i];
            auto &s = strings[i];
            
            text->set_origin(Vec2(0, 0.5));
            
            if(i >= positions.size())
                positions.push_back(Vec2());
            auto target = positions[i];
            
            if(s->pos.y > y) {
                offset.x = _settings.margins.x;
                
                auto current_height = Base::default_line_spacing(prev_font);
                for(size_t j=row_start; j<i; ++j) {
                    texts[j]->set_pos(Vec2(texts[j]->pos().x, offset.y + current_height * 0.5));
                }
                
                offset.y += Base::default_line_spacing(prev_font);//sf_line_spacing(s->font);
                y++;
                
                prev_font = s->font;
                row_start = i;
                
            } else if(s->font.size > prev_font.size)
                prev_font = s->font;
            
            target = Vec2(offset.x, offset.y);
            text->set_pos(target);
            //advance_wrap(*text);
            
            offset.x += text->text_bounds().width + text->text_bounds().x;
        }
        
        auto current_height = Base::default_line_spacing(prev_font);
        for(size_t j=row_start; j<texts.size(); ++j) {
            texts[j]->set_pos(Vec2(texts[j]->pos().x, offset.y + current_height * 0.5));
        }
        
        //end();
        
        set_content_changed(true);
    }
}
