#include <commons.pc.h>
#include <processing/LuminanceGrid.h>
#include <misc/Image.h>
#include <misc/colors.h>

#include <misc/Timer.h>
#include <misc/GlobalSettings.h>

namespace cmn {
    IMPLEMENT(CrashProgram::do_crash) = false;
    IMPLEMENT(CrashProgram::crash_pid);
    IMPLEMENT(CrashProgram::main_pid);
    
    void check_conditions(std::vector<HorizontalLine>& array) {
        if(array.empty())
            return;
        
        auto check_obj = [](HorizontalLine&, bool cond, const std::string& cond_name) {
            if(!cond) {
                FormatError("The program is not pleased. ",cond_name);
            }
        };
#define _assert(obj, COND) { check_obj(obj, (COND), #COND); }
        
        std::string wrong;
        
        for (size_t i=0; i<array.size()-1; i++) {
            auto &a = array.at(i);
            auto &b = array.at(i+1);
            
            _assert(a, a.x1 >= a.x0);
            _assert(b, b.x1 >= b.x0);
            
            _assert(a, a.y <= b.y);
            _assert(a, !a.overlap(b));
            _assert(a, a.y != b.y || a.x1+1 < b.x0);
        }
    }
    
    void HorizontalLine::repair_lines_array(std::vector<HorizontalLine> &ls, std::vector<uchar>& pixels)
    {
        std::set<HorizontalLine> lines(ls.begin(), ls.end());
        std::vector<uchar> corrected;
        corrected.reserve(pixels.size());
        if(lines.empty())
            return;
        
        auto prev = lines.begin();
        auto it = prev;
        ++it;
        auto pxptr = pixels.data();
        
        std::vector<HorizontalLine> result{ls.front()};
        corrected.insert(corrected.end(), pixels.begin(), pixels.begin() + ptr_safe_t(ls.front().x1) - ptr_safe_t(ls.front().x0) + 1);
        
        for(; it != lines.end();) {
            if(result.back().y == it->y) {
                if(result.back().x1 >= it->x0) {
                    // they do overlap in x and y
                    if(result.back().x0 > it->x0) {
                        auto offset = ptr_safe_t(result.back().x0) - ptr_safe_t(it->x0);
                        // need to add [offset] pixels to the front
                        corrected.insert(corrected.end() - (ptr_safe_t(result.back().x1) - ptr_safe_t(result.back().x0) + 1), pxptr, pxptr + offset);
                    }
                    
                    if(result.back().x1 < it->x1) {
                        auto offset = ptr_safe_t(it->x1) - ptr_safe_t(result.back().x1);
                        corrected.insert(corrected.end(), pxptr + ptr_safe_t(it->x1) - ptr_safe_t(it->x0) + 1 - offset, pxptr + ptr_safe_t(it->x1) - ptr_safe_t(it->x0) + 1);
                    }
                    
                    result.back() = result.back().merge(*it);
                    it = lines.erase(it);
                    
                    pxptr = pxptr + ptr_safe_t(it->x1) - ptr_safe_t(it->x0) + 1;
                    continue;
                }
                
            } else if(result.back().y > it->y)
                throw U_EXCEPTION("Cannot repair ",result.back().y," > ",it->y,"");
            
            prev = it;
            result.push_back(*it);
            
            auto start = pxptr;
            pxptr = pxptr + ptr_safe_t(it->x1) - ptr_safe_t(it->x0) + 1;
            corrected.insert(corrected.end(), start, pxptr);
            ++it;
        }
        
        ls = result;
        
#ifndef NDEBUG
        ptr_safe_t L = 0;
        for(auto &line : result) {
            L += ptr_safe_t(line.x1) - ptr_safe_t(line.x0) + 1;
        }
        
        assert(L == corrected.size());
#endif
    }
    
    void HorizontalLine::repair_lines_array(std::vector<HorizontalLine> &ls)
    {
        std::set<HorizontalLine> lines(ls.begin(), ls.end());
        if(lines.empty())
            return;
        
        auto prev = lines.begin();
        auto it = prev;
        ++it;
        
        std::vector<HorizontalLine> result{ls.front()};
        
        for(; it != lines.end();) {
            if(result.back().y == it->y) {
                if(result.back().x1 >= it->x0) {
                    // they do overlap in x and y
                    result.back() = result.back().merge(*it);
                    it = lines.erase(it);
                    continue;
                }
                
            } else if(result.back().y > it->y)
                throw U_EXCEPTION("Cannot repair ",result.back().y," > ",it->y,"");
            
            prev = it;
            result.push_back(*it);
            
            ++it;
        }
        
        ls = result;
    }
    
    enum InsertStatus {
        NONE = 0,
        ADDED,
        MERGED
    };
    
    InsertStatus insert(std::vector<HorizontalLine>& array, const HorizontalLine& p) {
#if !ORDERED_HORIZONTAL_LINES
        array.push_back(p);
#else
        InsertStatus status = NONE;
        
        // find best insertion spot
        uint64_t index = std::numeric_limits<long_t>::max();
        long_t k;
        for (k=long_t(array.size()-1u); k>=0; k--) {
            auto &c = array.at(k);
            
            if(c.y < p.y) {
                index = k + 1;
                array.insert(array.begin() + index, p);
                status = ADDED;
                break;
                
            }
            else if(c.y > p.y)
                continue;
            else if (c.overlap_x(p)) {
                // lines intersect. merge them.
                c = c.merge(p);
                
                status = MERGED;
                index = k;
                break;
                
            } else if(c.x0 <= p.x0) {
                index = k + 1;
                array.insert(array.begin() + index, p);
                
                status = ADDED;
                break;
            }
        }
        
        if (status == NONE && k <= 0) {
            array.insert(array.begin(), p);
            index = 0;
            status = ADDED;
            
        } else if (status == NONE) {
            index = array.size();
            array.push_back(p);
            status = ADDED;
        }
        
        bool change = false;
        do {
            change = false;
            
            for (long_t k = long_t(index) - 1; k<long_t(array.size())-1 && k < long_t(index) + 1; k++) {
                if(k < 0)
                    continue;
                
                auto &obj0 = array.at(k);
                auto &obj1 = array.at(k+1);
                
                if (obj0.overlap(obj1)) {
                    obj0 = obj0.merge(obj1);
                    array.erase(array.begin() + k + 1);
                    index = k;
                    change = true;
                    k--;
                }
            }
            
        } while(change);
        
        //check_conditions(array);
        return status;
        
#endif
    }
    
#if ORDERED_HORIZONTAL_LINES
#if true
    void dilate(std::vector<HorizontalLine>& array, int times, int max_cols, int max_rows) {
        if(array.empty())
            return;
        
        assert(times == 1);
        
        std::vector<HorizontalLine> ret;
        ret.reserve(array.size() * 3);
        
        auto it_current = ret.end(), it_before = ret.end();
        HorizontalLine *ptr_current = array.data(), *ptr_before = NULL;
        
        auto ptr = array.data();
        int current_y = array[0].y;
        bool previous_set = false;
        HorizontalLine previous;
        
        auto expand_row_with_row = [&ret](HorizontalLine *s0, HorizontalLine *e0,
                                          decltype(ret)::iterator s1, decltype(ret)::iterator e1,
                                          ushort insert_y)
        -> decltype(ret)::iterator
        {
            auto it = s1;
            HorizontalLine p;
            
            while(it != e1 && s0 < e0) {
                // x-pand multiple times...
                if(it->x0 > s0->x1+1) {
                    p = *s0;
                    p.y = insert_y;
                    
                    assert(ret.capacity() >= ret.size()+1);
                    it = ret.insert(it, p)+1;
                    e1++;
                    s0++;
                    
                } else if(it->x1 >= s0->x0-1) {
                    // merge...
                    *it = it->merge(*s0);
                    s0++;
                    
                } else {
                    ++it;
                }
            }
            
            while(s0 < e0) {
                p = *s0;
                p.y = insert_y;
                
                assert(ret.capacity() >= ret.size()+1);
                it = ret.insert(it, p)+1;
                e1++;
                s0++;
            }
            
            return e1;
        };
        
        //! this function is called whenever a row is completed
        auto complete_row = [&]() {
            if(previous_set) {
                ret.insert(ret.end(), previous);
                previous_set = false;
            }
            
            if(max_rows > 0 && current_y >= max_rows)
                return;
            
            // expand previous row into current row
            if(ptr_before) {
                if(current_y == ptr_before->y+1)
                    expand_row_with_row(ptr_before, ptr_current, it_current, ret.end(), current_y);
                else {
                    it_before = it_current;
                    it_current = expand_row_with_row(ptr_before, ptr_current, it_current, it_current, ptr_before->y+1);
                }
            }
            
            // expand current row into previous row
            if(current_y-1 >= 0) {
                if(it_before->y != current_y-1)
                    it_before = it_current;
                it_current = expand_row_with_row(ptr_current, ptr, it_before, it_current, current_y-1);
            }
            
            ptr_before = ptr_current;
            ptr_current = ptr;
            it_before = it_current;
            it_current = ret.end(); // will be first of current row
            
            if(ptr != array.data()+array.size())
                current_y = ptr->y;
            else
                current_y++;
        };
        
        // expand in x-direction
        for (size_t i=0; i<array.size(); i++, ptr++) {
            if(ptr->y != current_y) {
                complete_row();
            }
            
            if(previous_set && previous.x1 >= int(ptr->x0)-times) {
                // merge with previous line
                previous.x1 = max_cols > 0 
                    ? min(ptr_safe_t(ptr->x1)+ptr_safe_t(times), ptr_safe_t(max_cols-1)) 
                    : ptr_safe_t(ptr->x1)+times;
                
            } else {
                if(previous_set)
                    ret.insert(ret.end(), previous);
                
                previous = *ptr;
                previous.x0 -= min((ptr_safe_t)previous.x0, (ptr_safe_t)times); // expand left
                
                // expand right
                previous.x1 = max_cols > 0
                    ? min(ptr_safe_t(previous.x1)+times, ptr_safe_t(max_cols-1))
                    : ptr_safe_t(previous.x1)+times;
                
                previous_set = true;
            }
        }
        
        complete_row(); // add current row to previous row and vice-versa
        complete_row(); // copy/paste last row
        
        std::swap(array, ret);
    }
#else
    void dilate(std::vector<HorizontalLine>& array, int times, int max_cols, int max_rows) {
        int current_y = array.empty() ? 0 : array.front().y;
        Timer timer;
        
        std::vector<HorizontalLine> ret;
        ret.reserve(array.size()*1.25);
        
        size_t prev_start = 0;
        size_t prev_end = 0;
        size_t current_start = 0;
        
        //! Offset in case values might be lower than zero
        size_t offset = 0;
        
        auto inc_offset = [&]() {
            for (auto &r : ret) {
                r.y++;
            }
            
            offset++;
        };
        
        for (size_t i=0; i<array.size(); i++) {
            auto p = array[i];
            p.y += offset;
            
            if (p.y != current_y) {
                for (size_t j=prev_start; j<=prev_end; j++) {
                    auto p = array[j];
                    p.y++;
                    
                    insert(ret, p);
                }
                
                prev_start = current_start;
                prev_end = i - 1;
                
                current_start = i;
                current_y = p.y;
            }
            
            if(i == array.size())
                break;
            
            // for the first row
            if(current_start == prev_start) {
                auto c = p;
                if(c.y == 0) {
                    inc_offset();
                    c.y++;
                    p.y++;
                }
                c.y--;
                
                insert(ret, c);
                
            } else {
                // every other row
                auto c = p;
                if(c.y == 0) {
                    inc_offset();
                    c.y++;
                    p.y++;
                }
                c.y--;
                
                insert(ret, c);
            }
            
            // dilate in x-direction
            if (p.x0 > 0)
                p.x0--;
            p.x1++;
            
            insert(ret, p);
        }
        
        auto p = array.back();
        p.y+=offset;
        p.y++;
        insert(ret, p);
        
        array = ret;
        
        assert(USHRT_MAX > offset);
    }
#endif
#endif
    
    cv::Rect2i lines_dimensions(const std::vector<HorizontalLine>& lines) {
        float mx = FLT_MAX, my = FLT_MAX,
              px = -FLT_MAX, py = -FLT_MAX;
        
        for (auto &l : lines) {
            if(mx > l.x0)
                mx = l.x0;
            if(my > l.y)
                my = l.y;
            
            if(px < l.x1)
                px = l.x1;
            if(py < l.y)
                py = l.y;
        }
        
        float w = px - mx + 1,
        h = py - my + 1;
        
        return cv::Rect2i(mx, my, w, h);
    }
}

#include <misc/SpriteMap.h>
namespace cmn {
    namespace sprite {
        enum SupportedDataTypes {
            INT,
            LONG,
            FLOAT,
            STRING,
            VECTOR,
            BOOL,
            
            INVALID
        };
        
        SupportedDataTypes estimate_datatype(const std::string& value) {
            return INVALID;
            
            if(value.empty())
                return INVALID;
            
            if(utils::beginsWith(value, '"') && utils::endsWith(value, '"'))
                return STRING;
            if(utils::beginsWith(value, '\'') && utils::endsWith(value, '\''))
                return STRING;
            
            if(utils::beginsWith(value, '[') && utils::endsWith(value, ']'))
                return VECTOR;
            
            if((value.at(0) >= '0' && value.at(0) <= '9')
               || value.at(0) == '-' || value.at(0) == '+')
            {
                if(utils::contains(value, '.') || utils::endsWith(value, 'f'))
                    return FLOAT;
                if(utils::endsWith(value, 'l'))
                    return LONG;
                
                return INT;
            }
            
            if(value == "true" || value == "false")
                return BOOL;
            
            return INVALID;
        }
        
        std::set<std::string> parse_values(MapSource, Map& map, std::string str, const sprite::Map* additional, const std::vector<std::string>& exclude, const std::map<std::string, std::string>& deprecations) {
            str = utils::trim(str);
            if(str.empty())
                return {};
            
            std::set<std::string> added;
            
            if(utils::beginsWith(str, '{') && utils::endsWith(str, '}'))
                str = str.substr(1, str.length()-2);
            else
                throw U_EXCEPTION("Malformed map string ",utils::ShortenText(str, 1000));
            
            auto parts = util::parse_array_parts(str);
            for (auto &p : parts) {
                auto key_value = util::parse_array_parts(p, ':');
                if(key_value.empty())
					continue;
                auto &key = key_value[0];

                std::string value;
                if(key_value.size() > 1) value = key_value[1];
                
                if(!key.empty() && (key[0] == '\'' || key[0] == '"') && key[0] == key[key.length()-1])
                    key = key.substr(1, key.length()-2);
                if(not value.empty() 
                    && ((value.front() == '\"' && value.back() == '\"') 
                        || (value.front() == '\'' && value.back() == '\'')))
                    value = util::unescape(value.substr(1, value.length()-2u));

                if (deprecations.contains(key)) {
                    key = deprecations.at(key);
                }

                if (contains(exclude, key))
                    continue;

                if(map.has(key)) {
                    // try to set with existing type
                    map[key].get().set_value_from_string(value);
                } else if(additional && additional->has(key)) {
                    additional->at(key).get().copy_to(map);
                    //[key] = additional->docs.at(key);
                    map[key].get().set_value_from_string(value);
                } else {
                    // key is not yet present in the map, estimate type
                    auto type = estimate_datatype(value);
                    
                    switch (type) {
                        case STRING: {
                            if(value.length() && (value.at(0) == '"' || value.at(0) == '\''))
                                value = value.substr(1, value.length()-1);
                            if(value.length() && (value.at(value.length()-1) == '"' || value.at(value.length()-1) == '\''))
                                value = value.substr(0, value.length()-1);
                            map[key] = value;
                            
                            break;
                        }
                            
                        case INT:
                            map[key] = std::stoi(value);
                            break;
                        case FLOAT:
                            map[key] = std::stof(value);
                            break;
                        case LONG:
                            map[key] = std::stol(value);
                            break;
                            
                        case VECTOR:
                            if(!GlobalSettings::is_runtime_quiet(&map)) {
#ifndef NDEBUG
                                FormatWarning("(Key ",key,") Vector not yet implemented.");
#endif
                                continue;
                            }
                            break;
                            
                        case INVALID:
                            if(!GlobalSettings::is_runtime_quiet(&map)) {
                                FormatWarning("Data of invalid type ", utils::ShortenText(value, 1000)," for key ",key);
                                continue;
                            }
                            break;
                            
                        case BOOL:
                            map[key] = value == "true" ? true : false;
                            break;
                            
                        default:
                            break;
                    }
                    
                }
                
                added.insert(key);
            }
            
            /*Print("Added: ", added);
            for(auto key : added) {
                auto c = map[key].get().valueString();
                Print(key.c_str(), " = ", c.c_str());
            }*/
            return added;
        }
        
        Map parse_values(sprite::MapSource source, std::string str, const sprite::Map* additional) {
            Map map;
            parse_values(source, map, str, additional);
            return map;
        }
    }

std::string HorizontalLine::toStr() const {
    return "HL("+std::to_string(y)+" "+std::to_string(x0)+","+std::to_string(x1)+")";
}

    /*uint8_t required_channels(ImageMode mode) {
        switch (mode) {
            case ImageMode::GRAY:
                return required_channels<ImageMode::GRAY>();
            case ImageMode::R3G3B2:
                return required_channels<ImageMode::R3G3B2>();
            case ImageMode::RGB:
                return required_channels<ImageMode::RGB>();
            case ImageMode::RGBA:
                return required_channels<ImageMode::RGBA>();
                
            default:
                throw U_EXCEPTION("Unknown mode: ", (int)mode);
        }
    }*/

}
