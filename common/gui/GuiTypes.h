#ifndef _GUI_TYPES_H
#define _GUI_TYPES_H

#include <commons.pc.h>

#include <gui/types/Drawable.h>
#include <misc/Image.h>
#include <misc/colors.h>
#include <gui/ControlsAttributes.h>
#include <gui/CornerFlags.h>

#define CHANGE_SETTER(NAME) virtual void set_ ## NAME (const decltype( _settings. NAME ) & NAME) { \
if ( _settings. NAME == NAME ) { return; } _settings. NAME = NAME; set_dirty(); }

#define _CHANGE_SETTER(NAME) virtual void set_ ## NAME (const decltype( _ ## NAME ) & NAME) { \
if ( _ ## NAME == NAME ) { return; } _ ## NAME = NAME; set_dirty(); }

namespace cmn::gui {
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
    const std::vector<Vertex>* _transport{ nullptr };
    std::vector<Vertex> _points;
    std::vector<Vertex> _original_points;
    GETTER(PrimitiveType, primitive);
    GETTER(bool, size_calculated);
    GETTER(Float2_t, thickness);
    
public:
    VertexArray(const std::vector<Vertex>& p, PrimitiveType primitive, MEMORY memory, Type::Class type = Type::VERTICES);
    virtual ~VertexArray();
    
    void create(const std::vector<Vertex>& p, PrimitiveType primitive);
    
    template<typename... Vs>
    void create(PrimitiveType primitive, Vs... vertices) {
        const size_t N = sizeof...(vertices);
        bool dirty = _original_points.size() != N;
        if(dirty)
            _original_points.resize(N);
        
        size_t i=0;
        const auto update = [&](size_t index, auto& v){
            auto& ptr = _original_points[index];
            if(ptr != v) {
                ptr = v;
                dirty = true;
            }
        };
        (update(i++, vertices), ...);
        
        if(dirty || _primitive != primitive) {
            _transport = nullptr;
            //_points = nullptr;
            _primitive = primitive;
            _size_calculated = false;
            update_size();
        }
    }
    
    virtual const std::vector<Vertex>& points() {
        if(_transport)
            throw U_EXCEPTION("Vertices must be prepare()d before first use.");
        return _points;
    }
    
    void set_thickness(Float2_t t);
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
        ATTRIBUTE_ALIAS(Point_t, Vec2)
        ATTRIBUTE_ALIAS(Points_t, std::vector<Vec2>)
        ATTRIBUTE_ALIAS(Vertices_t, std::vector<Vertex>)
        NUMBER_ALIAS(Thickness_t, Float2_t)
    private:
        std::shared_ptr<std::vector<Vertex>> _processed_points;
        Color _line_clr;
        Float2_t _process_scale;
        GETTER(Float2_t, max_scale);
        
    public:
        template<typename... Args> Line(Args... args)
            : VertexArray({}, PrimitiveType::LineStrip, MEMORY::COPY, Type::LINE)
        {
            create(std::forward<Args>(args)...);
        }

        template<typename... Args> void create(Args... args) {
            set(Thickness_t{ 1 });

            if constexpr (requires { { set(std::forward<Args>(args)...) }->std::same_as<void>; }) {
                set(std::forward<Args>(args)...);
            }
            else {
                (set(std::forward<Args>(args)), ...);
            }
        }

    public:
        using Drawable::set;
        void set(const Vertices_t& A) {
            VertexArray::create(A, PrimitiveType::LineStrip);
        }
        void set(const Points_t& A, LineClr clr) {
            std::vector<Vertex> ps;
            ps.reserve(A.size());
            for(auto &a : A)
                ps.emplace_back(a, clr);
            VertexArray::create(std::move(ps), PrimitiveType::LineStrip);
            _line_clr = clr;
        }
        void set(const Points_t& A) {
            std::vector<Vertex> ps;
            ps.reserve(A.size());
            for(auto &a : A)
                ps.emplace_back(a, _line_clr);
            VertexArray::create(std::move(ps), PrimitiveType::LineStrip);
        }
        void set(Thickness_t t) {
            set_thickness(t);
        }
        void set(LineClr clr) {
            auto copy = _original_points;
            for(auto &c : copy)
                c.color() = ImU32(clr);
            VertexArray::create(std::move(copy), PrimitiveType::LineStrip);
            _line_clr = clr;
        }
        template<typename... Args>
        void addVertices(std::vector<Vertex>&vertices, Point_t pt, LineClr clr, Args... args)
        {
            vertices.emplace_back(pt, clr);  // Add the current pair

            if constexpr (sizeof...(args) > 0) {
                // Recur for the remaining arguments
                addVertices(vertices, args...);
            }
        }
        template<typename... Args>
        void set(Point_t p0, LineClr clr0, Point_t p1, LineClr clr1, Args... args) {
            std::vector<Vertex> ps;
            ps.emplace_back(p0, clr0);  // Add the first pair
            ps.emplace_back(p1, clr1);  // Add the second pair

            addVertices(ps, args...);
            VertexArray::create(std::move(ps), PrimitiveType::LineStrip);
        }
        void set(Point_t p0, Point_t p1, LineClr clr, Thickness_t t = Thickness_t{1}) {
            set_thickness(t);
            VertexArray::create(PrimitiveType::LineStrip, Vertex(p0, clr), Vertex(p1, clr));
        }
        void set(Point_t p0, LineClr clr0, Point_t p1, LineClr clr1, Thickness_t t = Thickness_t{ 1 }) {
            set_thickness(t);
            VertexArray::create(PrimitiveType::LineStrip, Vertex(p0, clr0), Vertex(p1, clr1));
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
        GETTER_PTR(std::shared_ptr<std::vector<Vec2>>, vertices);
        GETTER_PTR(std::shared_ptr<std::vector<Vec2>>, relative);
        GETTER(Color, fill_clr);
        GETTER(Color, border_clr);
        bool _size_calculated{false};
        GETTER(bool, show_points){false};
    public:
        static constexpr auto Class = Type::data::values::POLYGON;
        
    public:
        /*Polygon();
        Polygon(std::shared_ptr<std::vector<Vec2>> vertices, const Color& fill_clr = Transparent, const Color& line_clr = Transparent);
        Polygon(const std::vector<Vertex>& vertices, const Color& fill_clr = Transparent, const Color& line_clr = Transparent);*/
        
        template<typename... Args> Polygon(Args... args)
            : Drawable(Type::POLYGON)
        {
            create(std::forward<Args>(args)...);
        }

        template<typename... Args> void create(Args... args) {
            set(FillClr{ Transparent });
            set(LineClr{ Transparent });
            set(std::vector<Vertex>{});

            if constexpr (requires { { set(std::forward<Args>(args)...) }->std::same_as<void>; }) {
                set(std::forward<Args>(args)...);
            }
            else {
                (set(std::forward<Args>(args)), ...);
            }
        }
        
        using Drawable::set;
        void set(FillClr clr) { set_fill_clr(clr); }
        void set(LineClr clr) { set_border_clr(clr); }
        void set(const std::vector<Vertex>& v) { set_vertices(v); }
        void set(const std::vector<Vec2>& v) { set_vertices(v); }
        
        void set_fill_clr(const Color& clr);
        void set_border_clr(const Color& clr);
        void set_vertices(const std::vector<Vec2>& vertices);
        void set_vertices(const std::vector<Vertex>& vertices);
        void set_show_points(bool show_points) {
            if(show_points == _show_points)
                return;
            _show_points = show_points;
            set_dirty();
        }
        
        /*void create(const std::shared_ptr<std::vector<Vec2>>& vertices, const Color& fill_clr = Transparent, const Color& line_clr = Transparent)
        {
            set_fill_clr(fill_clr);
            set_border_clr(line_clr);
            set_vertices(*vertices);
        }*/
        
    protected:
        bool swap_with(Drawable* d) override;
        void update_size();
        bool in_bounds(Float2_t x, Float2_t y) override;
    };

class Rect final : public Drawable {
public:
    static constexpr auto Class = Type::data::values::RECT;
private:
    struct Settings {
        Color lineclr = Transparent,
              fillclr = Black;
        CornerFlags_t corners = CornerFlags_t{};
    } _settings;
    
public:
    const auto& lineclr() const { return _settings.lineclr; }
    const auto& fillclr() const { return _settings.fillclr; }
    auto corners() const { return _settings.corners; }
    
    Rect(const Rect&) = delete;
    Rect(Rect&&) = default;
    Rect& operator=(Rect&&) = default;
    Rect& operator=(const Rect&) = delete;
    
    template<typename... Args>
    Rect(Args... args) : gui::Drawable(Type::RECT)
    {
        create(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void create(Args... args) {
        _settings = {};
        
        (set(std::forward<Args>(args)), ...);
    }
    
public:
    using Drawable::set;
    void set(LineClr clr) { set_lineclr(clr); }
    void set(FillClr clr) { set_fillclr(clr); }
    void set(CornerFlags_t flags) {
        if(flags == _settings.corners)
            return;
        _settings.corners = flags;
        set_dirty();
    }
    
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
        set(ptr->corners());
        
        return true;
    }
};

    class Circle final : public Drawable {
    public:
        static constexpr auto Class = Type::data::values::CIRCLE;
    private:
        struct Settings {
            Float2_t radius = 1;
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
        
        void set_radius(Float2_t radius) {
            if(int(_settings.radius) == int(radius))
                return;
            
            _settings.radius = radius;
            set_size(Size2(_settings.radius * 2));
        }
        
        CHANGE_SETTER(line_clr)
        CHANGE_SETTER(fill_clr)
        
        bool in_bounds(Float2_t x, Float2_t y) override {
            auto size = global_bounds();
            return euclidean_distance(Vec2(x, y), size.pos() + size.size().mul(Vec2(1) - origin())) <= size.width * 0.5_F;
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
        NUMBER_ALIAS(Shadow_t, Float2_t)
        
    private:
        struct Settings {
            std::string txt;
            Color color = White;
            Font font;
            Shadow_t shadow{0};
            
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
            _settings = {};

            if constexpr(not contains_type<Origin, Args...>()) {
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
                        set(Origin(0, 0));
                        break;
                }
            }
            
            (set(std::forward<Args>(args)), ...);
            
            
        }
        
        virtual ~Text() {}
        
    public:
        using Drawable::set;
        void set(const Str& txt) { set_txt(txt); }
        void set(const TextClr& color) { set_color(color); }
        void set(const Font& font) { set_font(font); }
        void set(const Shadow_t& shadow) {
            if(_settings.shadow == shadow)
                return;
            _settings.shadow = shadow;
            set_dirty();
        }
        
    public:
        const auto& font() const { return _settings.font; }
        const auto& color() const { return _settings.color; }
        const auto& txt() const { return _settings.txt; }
        const auto& shadow() const { return _settings.shadow; }
        
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
            else if(font.align == Align::VerticalCenter)
                set_origin(Vec2(0,0.5));
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
                Print("Destructed");
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
        GETTER(std::string, url);
        Ptr _source;
        GETTER(Color, color);
        GETTER(bool, cut_border){false};
        
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
            _cut_border = false;
        }
        
        _CHANGE_SETTER(url)
        _CHANGE_SETTER(color)
        _CHANGE_SETTER(cut_border)
        
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
            _cut_border = ptr->_cut_border;
            
            if(!(*ptr->_source == *_source)) {
                std::swap(ptr->_source, _source);
                clear_cache();set_dirty();
            }
            
            set_scale(ptr->_scale);
            
            return true;
        }
    };

template<typename T>
void reduce_vertex_line(const std::vector<T>& points, std::vector<T>& array, Float2_t threshold)
{
    array.clear();
    
    if(points.empty())
        return;
    
    T last_added_point(points.front());
    array.push_back(last_added_point);
    
    if(points.size() == 1)
        return;
    
    Vec2 previous_vec;
    if constexpr(std::same_as<Vec2, T>)
        previous_vec = (last_added_point - points.back()).normalize();
    else
        previous_vec = (last_added_point.position() - points.back().position()).normalize();
    
    Bounds bounds(FLT_MAX, FLT_MAX, 0, 0);
    for(auto &v : points) {
        const Vec2* pt;
        if constexpr(std::same_as<T, Vec2>) {
            pt = &v;
        } else
            pt = &v.position();
        if(pt->x < bounds.x) bounds.x = pt->x;
        if(pt->y < bounds.y) bounds.y = pt->y;
        if(pt->x > bounds.width) bounds.width = pt->x;
        if(pt->y > bounds.height) bounds.height = pt->y;
    }
    
    bounds << Size2(bounds.size() - bounds.pos());
    Float2_t dim = bounds.size().length();
    Float2_t cumlen = 0;
    Vec2 prev_vertex = last_added_point;
    
    for (size_t i=1; i<points.size()-1; i++) {
        T p1(points.at(i));
        
        Vec2 line;
        if constexpr(std::same_as<Vertex, T>) {
            line = p1.position() - last_added_point.position();
            cumlen += (p1.position() - prev_vertex).length();
            prev_vertex = p1.position();
        } else {
            line = p1 - last_added_point;
            cumlen += (p1 - prev_vertex).length();
            prev_vertex = p1;
        }
        
        auto len = line.length();
        line = line / len;
        
        if(abs(line.x) < 127 && abs(line.y) < 127) {
            // returns [-1,1] values from 180°-0° (upper half of the unit circle)
            // 90°-180° is < 0, while 0°-90° is >= 0
            // move it to [-2,0], take absolute and multiply by 0.5, so we get 0-180° -> [0,1]
            Float2_t a = cmn::abs(previous_vec.dot(line) - 1) * 0.5_F;
            if (cumlen < dim * threshold * 0.005_F && a < threshold * 0.1_F) {
                if constexpr(std::same_as<Vertex, T>) {
                    auto color_diff = p1.clr().diff(last_added_point.clr()) / 4_F / 255_F;
                    if(color_diff < threshold * 0.01_F)
                        continue;
                } else
                    continue;
            }
        }
        
        cumlen = 0_F;
        
        //change[3] = min_alpha;
        last_added_point = p1;
        previous_vec = line;
        array.push_back(last_added_point);
    }
    
    array.push_back(points.back());
    
}


}

#endif

