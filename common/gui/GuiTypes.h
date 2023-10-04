#ifndef _GUI_TYPES_H
#define _GUI_TYPES_H

#include <misc/defines.h>

#include <gui/types/Drawable.h>
#include <misc/Image.h>
#include <gui/colors.h>
#include <gui/ControlsAttributes.h>

#define CHANGE_SETTER(NAME) virtual void set_ ## NAME (const decltype( _settings. NAME ) & NAME) { \
if ( _settings. NAME == NAME ) return; _settings. NAME = NAME; set_dirty(); }

#define _CHANGE_SETTER(NAME) virtual void set_ ## NAME (const decltype( _ ## NAME ) & NAME) { \
if ( _ ## NAME == NAME ) return; _ ## NAME = NAME; set_dirty(); }

namespace gui {
    enum class PrimitiveType
    {
        Points,
        Lines,
        LineStrip,
        Triangles,
        TriangleStrip
    };

using namespace attr;

class VertexArray : public Drawable {
public:
    enum MEMORY {
        TRANSPORT,
        COPY
    };
    
protected:
    const std::vector<Vertex>* _transport;
    std::shared_ptr<std::vector<Vertex>> _points;
    std::vector<Vertex> _original_points;
    GETTER(PrimitiveType, primitive)
    GETTER(bool, size_calculated)
    GETTER(float, thickness)
    
public:
    VertexArray(const std::vector<Vertex>& p, PrimitiveType primitive, MEMORY memory, Type::Class type = Type::VERTICES);
    virtual ~VertexArray();
    
    void create(const std::vector<Vertex>& p, PrimitiveType primitive) {
        if(_original_points != p || primitive != _primitive) {
            _original_points = p;
            _points = nullptr;
            _transport = nullptr;
            _primitive = primitive;
            update_size();
        }
    }
    
    virtual const std::vector<Vertex>& points() {
        if(_transport)
            throw U_EXCEPTION("Vertices must be prepare()d before first use.");
        assert(_points);
        
        return *_points;
    }
    
    void set_thickness(float t);
    bool operator!=(const VertexArray& other) const;
    std::ostream &operator <<(std::ostream &os) override;
    void set_pos(const Vec2&) override;
    void set_size(const Size2&) override;
    void set_bounds(const Bounds&) override;

    std::vector<Vertex>& change_points() {
        return _original_points;
    }
    void confirm_points();
    //virtual bool is_same_type(Drawable* d) const { return dynamic_cast<Vertices*>(d) != nullptr; }
    
    bool swap_with(Drawable* d) override;
    
    friend class Section;
    friend class Entangled;
    
    //! Moves points from the _transport array to a copied _points array.
    virtual void prepare();
    
    void set_parent(SectionInterface* p) final override {
        if(p != parent()) {
            Drawable::set_parent(p);
            
            if(p && _transport)
                prepare();
        }
    }
    
protected:
    void update_size();
    virtual std::string vertex_sub_type() const = 0;
};
    
class Vertices final : public VertexArray {
public:
    static constexpr auto Class = Type::data::values::VERTICES;
    
public:
    Vertices(const std::vector<Vertex>& p, PrimitiveType primitive, MEMORY memory = MEMORY::COPY);
    Vertices(const Vec2& p0, const Vec2& p1, const Color& clr);
    
    ~Vertices() {}
    
    void create(const std::vector<Vertex>& p, PrimitiveType primitive) {
        VertexArray::create(p, primitive);
    }
    void create(const Vec2& p0, const Vec2& p1, const Color& clr) {
        VertexArray::create(std::vector<Vertex>{Vertex(p0, clr), Vertex(p1, clr)}, PrimitiveType::LineStrip);
    }
    
protected:
    std::string vertex_sub_type() const override { return "Vertices"; }
};
    
    class Line final : public VertexArray {
    public:
        static constexpr auto Class = Type::data::values::LINE;
    private:
        std::shared_ptr<std::vector<Vertex>> _processed_points;
        float _process_scale;
        GETTER(float, max_scale)
        
    public:
        Line(const Vec2& pos0, const Vec2& pos1, const Color& color, float t, MEMORY memory = COPY)
            : Line({Vertex(pos0, color), Vertex(pos1, color)}, t, memory)
        {}
        Line(const Vec2& pos0, const Vec2& pos1, const Color& color, const Vec2& scale = Vec2(1), float t = 1)
            : Line({ Vertex(pos0, color), Vertex(pos1, color) }, t, MEMORY::COPY, scale)
        {}
        
        Line(const std::vector<Vertex>& p, float t, MEMORY memory = TRANSPORT, const Vec2& scale = Vec2(1))
            : VertexArray(p, PrimitiveType::LineStrip, memory, Type::LINE),
              _processed_points(NULL), _process_scale(-1),
              _max_scale(1)
        {
            if(_thickness != 1) set_thickness(t);
            set_scale(scale);
        }
        
        void create(const std::vector<Vertex>& p, float t, MEMORY = TRANSPORT, const Vec2& scale = Vec2(1)) {
            set_thickness(t);
            set_scale(scale);
            VertexArray::create(p, PrimitiveType::LineStrip);
        }
        
        void create(const Vec2& pos0, const Vec2& pos1, const Color& color, const Vec2& scale = Vec2(1), float t = 1) {
            set_scale(scale);
            set_thickness(t);
            VertexArray::create(std::vector<Vertex>{Vertex(pos0, color), Vertex(pos1, color)}, PrimitiveType::LineStrip);
        }
        void create(const Vec2& pos0, const Vec2& pos1, const Color& color, float t, MEMORY = MEMORY::COPY, const Vec2&scale = Vec2(1)) {
            set_scale(scale);
            set_thickness(t);
            VertexArray::create(std::vector<Vertex>{Vertex(pos0, color), Vertex(pos1, color)}, PrimitiveType::LineStrip);
        }
        
        const std::vector<Vertex>& points() override final;
        std::ostream &operator <<(std::ostream &os) override;
        decltype(_processed_points) processed_points() const { return _processed_points; }
        const decltype(_points)& raw_points() const { return _points; }
        
    protected:
        bool swap_with(Drawable* d) override;
        void update_bounds() override;
        void prepare() override;
        
        //virtual bool is_same_type(Drawable* d) const override { return dynamic_cast<Line*>(d) != nullptr; }
        
        //! Moves points from the _transport array to a copied _raw_points array.
        //void prepare() override;
        std::string vertex_sub_type() const override { return "Line"; }
    };
    
    class Polygon : public Drawable {
        GETTER_PTR(std::shared_ptr<std::vector<Vec2>>, vertices)
        GETTER_PTR(std::shared_ptr<std::vector<Vec2>>, relative)
        GETTER(Color, fill_clr)
        GETTER(Color, border_clr)
        bool _size_calculated;
    public:
        static constexpr auto Class = Type::data::values::POLYGON;
        
    public:
        Polygon();
        Polygon(std::shared_ptr<std::vector<Vec2>> vertices, const Color& fill_clr = Transparent, const Color& line_clr = Transparent);
        Polygon(const std::vector<Vertex>& vertices, const Color& fill_clr = Transparent, const Color& line_clr = Transparent);
        void set_fill_clr(const Color& clr);
        void set_border_clr(const Color& clr);
        void set_vertices(const std::vector<Vec2>& vertices);
        
        void create(const std::shared_ptr<std::vector<Vec2>>& vertices, const Color& fill_clr = Transparent, const Color& line_clr = Transparent)
        {
            set_fill_clr(fill_clr);
            set_border_clr(line_clr);
            set_vertices(*vertices);
        }
        
    protected:
        bool swap_with(Drawable* d) override;
        void update_size();
        bool in_bounds(float x, float y) override;
    };
    
class Rect final : public Drawable {
public:
    static constexpr auto Class = Type::data::values::RECT;
private:
    struct Settings {
        Color lineclr = Transparent,
              fillclr = Black;
    } _settings;
    
public:
    const auto& lineclr() const { return _settings.lineclr; }
    const auto& fillclr() const { return _settings.fillclr; }
    
    template<typename... Args>
    Rect(Args... args) : gui::Drawable(Type::RECT)
    {
        create(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void create(Args... args) {
        (set(std::forward<Args>(args)), ...);
    }
    
public:
    using Drawable::set;
    void set(LineClr clr) { set_lineclr(clr); }
    void set(FillClr clr) { set_fillclr(clr); }
    
public:
    virtual ~Rect() {}
    
    CHANGE_SETTER(lineclr)
    CHANGE_SETTER(fillclr)
    
    std::ostream &operator <<(std::ostream &os) override;
    
protected:
    bool swap_with(Drawable* d) override {
        if(!Drawable::swap_with(d))
            return false;
        
        auto ptr = static_cast<const Rect*>(d);
        set_fillclr(ptr->fillclr());
        set_lineclr(ptr->lineclr());
        
        return true;
    }
};

    class Circle final : public Drawable {
    public:
        static constexpr auto Class = Type::data::values::CIRCLE;
    private:
        struct Settings {
            float radius = 1;
            Color line_clr = White;
            Color fill_clr = Transparent;
        } _settings;
        
    public:
        template<typename... Args>
        Circle(Args... args)
            : gui::Drawable(Type::CIRCLE, Bounds(0, 0, 2, 2), Origin(0.5))
        { create(std::forward<Args>(args)...); }
        
        template<typename... Args>
        static std::shared_ptr<Circle> MakePtr(Args... args) {
            return std::make_shared<Circle>(std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        void create(Args... args) {
            (set(std::forward<Args>(args)), ...);
            
            if constexpr(!pack_contains<Origin, Args...>()) {
                set_origin(Vec2(0.5));
            }
        }
        
    public:
        using Drawable::set;
        void set(Radius radius) { set_radius(radius); }
        void set(FillClr clr) { set_fill_clr(clr); }
        void set(LineClr clr) { set_line_clr(clr); }
        
    private:
        void init();
        
    public:
        virtual ~Circle() {}
        
        auto radius() const { return _settings.radius; }
        const auto& fill_clr() const { return _settings.fill_clr; }
        const auto& line_clr() const { return _settings.line_clr; }
        
        void set_radius(float radius) {
            if(int(_settings.radius) == int(radius))
                return;
            
            _settings.radius = radius;
            set_size(Size2(_settings.radius * 2));
        }
        
        CHANGE_SETTER(line_clr)
        CHANGE_SETTER(fill_clr)
        
        bool in_bounds(float x, float y) override {
            auto size = global_bounds();
            return euclidean_distance(Vec2(x, y), size.pos() + size.size().mul(Vec2(1) - origin())) <= size.width * 0.5f;
        }
        
        std::ostream &operator <<(std::ostream &os) override;
        
    private:
        bool swap_with(Drawable* d) override {
            if(!Drawable::swap_with(d))
                return false;
            
            auto ptr = static_cast<const Circle*>(d);
            set_radius(ptr->_settings.radius);
            set_line_clr(ptr->_settings.line_clr);
            set_fill_clr(ptr->_settings.fill_clr);
            
            return true;
        }
    };
    
    class Text final : public Drawable {
    public:
        static constexpr auto Class = Type::data::values::TEXT;
    private:
        struct Settings {
            std::string txt;
            Color color = White;
            Font font;
        } _settings;
        
        Bounds _text_bounds;
        bool _bounds_calculated{false};
        
    public:
        template<typename... Args> Text(Args... args)
            : gui::Drawable(Type::TEXT, Bounds(), Vec2())
        {
            create(std::forward<Args>(args)...);
        }
        
        template<typename... Args> void create(Args... args) {
            set(Origin(0, 0));
            
            (set(std::forward<Args>(args)), ...);
            
            switch(_settings.font.align) {
                case Align::Center:
                    set(Origin(0.5));
                    break;
                case Align::Right:
                    set(Origin(1, 0));
                    break;
                case Align::VerticalCenter:
                    set(Origin(0, 0.5));
                    break;
                    
                default:
                    break;
            }
        }
        
        virtual ~Text() {}
        
    public:
        using Drawable::set;
        void set(const Str& txt) { set_txt(txt); }
        void set(const Color& color) { set_color(color); }
        void set(const Font& font) { set_font(font); }
        
    public:
        const auto& font() const { return _settings.font; }
        const auto& color() const { return _settings.color; }
        const auto& txt() const { return _settings.txt; }
        
        void set_txt(const std::string& txt) {
            if(txt == _settings.txt)
                return;
            
            _settings.txt = txt;
            _bounds_calculated = false;
			set_bounds_changed();
        }
        
        CHANGE_SETTER(color)
        
        void set_font(const Font& font) {
            if(font == _settings.font)
                return;
            
            //if(font.align != _settings.font.align) {
            if(font.align == Align::Center)
                set_origin(Vec2(0.5, 0.5));
            else if(font.align == Align::Left)
                set_origin(Vec2());
            else if(font.align == Align::Right)
                set_origin(Vec2(1, 0));
            //}
            
            _settings.font = font;
            _bounds_calculated = false;
            set_bounds_changed();
        }
        
        std::ostream &operator <<(std::ostream &os) override;
        virtual std::string toStr() const override;
        
        const Bounds& text_bounds() {
            refresh_dims();
            return _text_bounds;
        }
        
        Size2 size() override {
            //update_bounds();
            refresh_dims();
            return Drawable::size();
        }
        
    private:
        //! Hide set_size method, which doesn't apply to Texts.
        //  Their size is determined by the text itself and automatically updated.
        void set_size(const Size2& size) override {
            Drawable::set_size(size);
        }
        
        void update_bounds() override {
            refresh_dims();
            Drawable::update_bounds();
        }
        
        
        bool swap_with(Drawable* d) override {
            if(d->type() != Type::TEXT)
                return false;
            
            auto ptr = static_cast<const Text*>(d);
            
            set_pos(d->pos());
            set_origin(d->origin());
            set_scale(d->scale());
            
            set_txt(ptr->_settings.txt);
            set_color(ptr->_settings.color);
            set_font(ptr->_settings.font);
            
            if(ptr->_bounds_calculated) {
                _bounds_calculated = true;
                _text_bounds = ptr->_text_bounds;
                set_size(Size2(_text_bounds.pos() + _text_bounds.size()));
            }
            
            return true;
        }
        
        void refresh_dims();
    };
    
    class ExternalImage : public Drawable {
    public:
        static constexpr auto Class = Type::data::values::IMAGE;
        using Ptr = Image::Ptr;
/*#ifdef NDEBUG
        struct ReturnRef {
        private:
            ExternalImage* source;
        public:
            ReturnRef(ExternalImage* source) : source(source) {}
            ~ReturnRef() {
                print("Destructed");
                source->clear_cache(); source->set_dirty();
            }
            operator Image& () {
                return *source->_source;
            }
        };
#else*/
        using ReturnRef = Image::Ptr&;
//#endif

    private:
        GETTER(std::string, url)
        Ptr _source;
        GETTER(Color, color)
        
    public:
        ExternalImage() : ExternalImage(Image::Make(), Vec2()) {}
        ExternalImage(ExternalImage&&) = default;
        ExternalImage(Ptr&& source, const Vec2& pos = Vec2(), const Vec2& scale = Vec2(1,1), const Color& color = Transparent);
        ~ExternalImage() { }
        ExternalImage(const ExternalImage& e) = delete;
        
        ExternalImage& operator=(ExternalImage&&) = default;
        
        void create(Ptr&& source, const Vec2& pos, const Vec2& scale = Vec2(1), const Color& color = Transparent) {
            set_source(std::move(source));
            set_pos(pos);
            set_scale(scale);
            set_color(color);
        }
        
        _CHANGE_SETTER(url)
        _CHANGE_SETTER(color)
        
        virtual const Image* source() const { return _source.get(); }
/*#ifdef NDEBUG
        ReturnRef unsafe_get_source() { return ReturnRef(this); }
#else*/
        Image& unsafe_get_source() { return *_source; }
//#endif
        //virtual void set_source(const Image& source);
        virtual void set_source(Ptr&& source);
        //void set_source(const cv::Mat& source);
        void set_bounds_changed() override;
        std::ostream &operator <<(std::ostream &os) override;
        void update_with(const gpuMat&);
        void update_with(const Image&);
        void update_with(Image::Ptr&&);
        Ptr exchange_with(Ptr&&);
        
        void updated_source();
        bool empty() const { return !_source || _source->size() == 0; }
        
    private:
        bool swap_with(Drawable* d) override {
            if(!Drawable::swap_with(d))
                return false;
            
            auto ptr = static_cast<ExternalImage*>(d);
            
            // only swap images with the same dimensions
            if(source() && ptr->source() && ptr->source()->dims != source()->dims)
                return false;
            
            set_url(ptr->_url);
            set_color(ptr->_color);
            
            if(!(*ptr->_source == *_source)) {
                std::swap(ptr->_source, _source);
                clear_cache();set_dirty();
            }
            
            set_scale(ptr->_scale);
            
            return true;
        }
    };
}

#endif

