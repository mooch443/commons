#include "DrawHTMLBase.h"
#include <misc/Base64.h>
#include <gui/types/StaticText.h>
#include <gui/DrawStructure.h>
#include <gui/Passthrough.h>

namespace cmn::gui {
    
    HTMLBase::HTMLBase() : _initial_draw(true), _size(1) { }

    void HTMLBase::set_window_size(Size2 size) {
        _size = size;
    }

    Bounds HTMLBase::get_window_bounds() const {
        return Bounds(0, 0, _size.width, _size.height);
    }

    void HTMLBase::set_window_bounds(Bounds bounds) {
		_size = bounds.size();
	}
    
    void HTMLBase::paint(gui::DrawStructure &s) {
        auto lock = GUI_LOCK(s.lock());
        if(_size.empty())
            _size = Size2(s.width(), s.height());
        
        s.before_paint(this);
        
        auto t = s.root().cached(this);
        if(!_initial_draw && t && !t->changed())
            return;
        
        _ss.str("");
        _ss << "{\"w\":"<<int(_size.width)<<",\"h\":"<<int(_size.height)<<",\"o\":{";
        
        std::vector<std::string> order;
        auto objects = s.collect();
        bool prepend_comma = false;
        
        std::function<std::string(Drawable*, bool)> fn = [&](Drawable*o, bool is_background = false)
        {
            auto d = o;
            o = o->type() == Type::SINGLETON
                    ? static_cast<SingletonObject*>(o)->ptr()
                    : o;
            while(o && o->type() == Type::PASSTHROUGH)
                o = static_cast<Fallthrough*>(o)->object().get();
            
            if(not o || o->type() == Type::POLYGON)
                return std::string();
            
            //bounds.combine(o->global_transform());
            //Vec2 scale(s.scale()); // gui::interface_scale()); // .div(Vec2(1, 1));
            //scale /= gui::interface_scale() * 0.5;
            Transform transform;
            //transform.scale(scale);
            
            if(is_background && o->parent() && o->parent()->type() == Type::ENTANGLED)
            {
                auto p = o->parent();
                if(p)
                    transform.combine(p->global_transform());
                
            } else {
                transform.combine(o->global_transform());
            }
            
            std::stringstream ss;
            
            auto ptr = o->cached(this);
            HTMLCache* cache = ptr ? (HTMLCache*)ptr : nullptr;
            if(!cache) {
                cache = (HTMLCache*)o->insert_cache(this, std::make_unique<HTMLCache>()).get();
            }
            
            if(_initial_draw && cache)
                cache->set_changed(true);
            
            if (!is_background && o->type() == Type::ENTANGLED) {
                bool prepend = false;
                Drawable* bg = static_cast<SectionInterface*>(o)->background();
                
                if(bg) {
                    auto str = fn(bg, true);
                    if(!str.empty()) {
                        prepend = true;
                        ss << str;
                    }
                }
                
                assert(!static_cast<Entangled*>(o)->begun());
                for(auto c : static_cast<Entangled*>(o)->children()) {
                    std::string str = fn(c, false);
                    
                    if(!str.empty()) {
                        if(prepend)
                            ss << ",";
                        else
                            prepend = true;
                        
                        ss << str;
                    }
                }
                
                if(!cache && !cache->changed())
                    return std::string();
                
                return ss.str();
            }
            
            order.push_back(std::to_string(size_t(o)));
            
            if(!is_background && cache && !cache->changed())
                return std::string();
            
            std::stringstream matrix;
#define MATRIX(I, J) transform.getMatrix()[I * 4 + J]
            
            matrix  << MATRIX(0, 0) << "," << MATRIX(0, 1) << ","
            << MATRIX(1, 0) << "," << MATRIX(1, 1) << ","
            << MATRIX(3, 0) << ","  << MATRIX(3, 1);
            std::string trans = matrix.str();
            
            ss  << "\"" << size_t(o) << "\":[" << (int)o->type().value()
                << ",[" << trans << "]"
                //<< "," << o->origin().x << "," << o->origin().y
                << ",";
            o->operator<<(ss);
            ss << "]";
            
            if(dynamic_cast<HasName*>(d))
                Print("Sending ", dynamic_cast<HasName*>(d)->name());
            
            if(cache) {
                cache->set_text(ss.str());
                cache->set_changed(false);
            }
            
            return ss.str();
        };
        
        for(size_t i=0; i<objects.size(); i++) {
            auto o = objects[i];
            
            try {
                //Transform tf;
                //tf.scale(Vec2(1/gui::interface_scale()));
                //tf.combine(o->global_transform());
                
                std::string object_str = fn(o, false);
                if(!object_str.empty()) {
                    if(prepend_comma)
                        _ss << ",";
                    else prepend_comma = true;
                    _ss << object_str;
                }
            } catch(const UtilsException& e) {
                Print("Skipping object that generated an error.");
            }
        }
        
        _ss << "},\"a\":";
        _ss << Meta::toStr<decltype(order)>(order) << "}";
        
        if(!t) {
            t = s.root().insert_cache(this, std::make_unique<HTMLCache>()).get();
        }
        t->set_changed(false);
        
        std::string str = _ss.str();
        _vec.assign(str.begin(), str.end());
        
        _initial_draw = false;
    }
    
    void HTMLBase::reset() {
        _initial_draw = true;
    }

Size2 HTMLBase::window_dimensions() const {
    return _size / gui::interface_scale(); //* gui::interface_scale() * 2;
}

}
