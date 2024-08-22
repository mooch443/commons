#ifndef _DRAWABLE_H
#define _DRAWABLE_H

#include <commons.pc.h>
//#include <gui/types/Basic.h>
#include <gui/Transform.h>
#include <gui/Event.h>
#include <misc/colors.h>
#include <gui/ControlsAttributes.h>

namespace cmn::gui {
    class Base;
    class SectionInterface;
    class DrawableCollection;

    using namespace attr;
    
    class DrawStructure;
    class CacheObject {
        GETTER(std::atomic_bool, changed);
        
    public:
        typedef std::unique_ptr<CacheObject> Ptr;
        CacheObject();
        virtual ~CacheObject();
        static size_t memory();
        virtual void set_changed(bool v) {
            _changed = v;
        }
    };
    
    ENUM_CLASS (Type,
        NONE,    VERTICES,  CIRCLE,
        RECT,    TEXT,      IMAGE,
        SECTION, SINGLETON, ENTANGLED,
        POLYGON, LINE,      PASSTHROUGH
    )
    
    Float2_t interface_scale();
    
    namespace hidden {
        class Global {
        private:
            Global() {}
            
        public:
            static Float2_t interface_scale;
        };
    }

    struct TopLeft { constexpr operator Vec2() const { return Vec2(0, 0); } };
    struct TopRight { constexpr operator Vec2() const { return Vec2(1, 0); } };
    struct BottomRight { constexpr operator Vec2() const { return Vec2(1, 1); } };
    struct BottomLeft { constexpr operator Vec2() const { return Vec2(0, 1); } };

    template<typename T>
    concept _has_origin = requires (T t) {
        { t.origin() } -> std::convertible_to<Vec2>;
    };
    
    class Drawable {
    public:
        typedef std::function<bool(Event)> event_handler_t;
        typedef std::function<void(Event)> event_handler_yes_t;
        typedef std::shared_ptr<event_handler_t> callback_handle_t;
        typedef std::function<void(void)> delete_function_t;
        typedef std::unique_ptr<delete_function_t> delete_function_ptr_t;
        typedef const delete_function_t* delete_function_handle_t;
        
        struct replace_t {};
        
        //! A color that is used in Drawables throughout the GUI
        //  as the accent / base color.
        static Color accent_color;
        
    protected:
        std::unordered_map<EventType, std::vector<callback_handle_t>> _event_handlers;
        std::vector<delete_function_ptr_t> _delete_handlers;
        
        //! Objects that contain cached values for the Base* objects
        //  using the Drawables
        robin_hood::unordered_map<const Base*, CacheObject::Ptr> _cache;
        
        std::unordered_map<std::string_view, std::tuple<void*, std::function<void(void*)>>> _custom_data;
        std::unordered_set<uchar> _custom_tags;
        
        GETTER(std::string, name);
        
    public:
        virtual void set_name(const std::string& name) {
            _name = name;
        }
        void add_custom_data(std::string_view key, void* data, std::function<void(void*)> deleter = [](void*){}) {
            _custom_data[key] = {data, deleter};
        }
        void remove_custom_data(std::string_view key) {
            auto it = _custom_data.find(key);
            if(it != _custom_data.end())
                _custom_data.erase(key);
        }
        void* custom_data(std::string_view key) const {
            auto it = _custom_data.find(key);
            if(it != _custom_data.end()) {
                return std::get<0>(it->second);
            }
            return NULL;
        }
        
        template<typename T> bool tagged(T tag) const { return _custom_tags.count((uint32_t)tag); }
        template<typename T> void tag(T t) { if(!tagged(t)) _custom_tags.insert((uint32_t)t); }
        template<typename T>
        void untag(T t) {
            auto it = _custom_tags.find((uint32_t)t);
            if(it != _custom_tags.end())
                _custom_tags.erase(it);
        }
        
    /**
     * ------------------
     * STRUCTURAL / BASIC
     * ------------------
     */
    protected:
        //! Type of this Drawable, indicating how to draw it
        GETTER(Type::Class, type);
        
        //! Children will be positioned relative to _parent and
        //  also transmit did_change events.
        GETTER_PTR(SectionInterface*, parent);
        
    /**
     * --------------------------
     * COORDINATES AND TRANSFORMS
     * --------------------------
     */
    protected:
        //! Origin in x/y direction goes from 0 to 1.
        //  If set to 0.5,0.5 the object will be centered
        //  on pos().
        GETTER(Vec2, origin);
        
        //! Rotation of the object and all children.
        //  Object is rotated around origin.
        GETTER(Float2_t, rotation);
        
        //! This is true, if this object (or one of its parents) has rotation != 0.
        GETTER(bool, has_global_rotation);
        
        //! The value of the user-defined rectangle.
        //  (local position / size)
        Bounds _bounds;
        
        //! If this object has been rendered in the last renderpass, this will be set to true.
        GETTER_SETTER_I(bool, was_visible, false);

        //! If the object is disabled, it will not be displayed.
        GETTER_I(bool, is_displayed, true);

        GETTER_I(bool, rendered, false);
        
        //! Whether or not this object is currenty playing an animation
        //! (and should thus be re-rendered continously):
        bool _animating = false;
        
    protected:
        //! If this is set to true, the next bounds(), rect()
        //  call needs to be (and will be) preceeded by
        //  update_bounds().
        bool _bounds_changed;
        
    protected:
        //! [Cached] The real position of the object,
        //  taking into account the origin.
        Vec2 _location;
        
        //! [Cached] Global position, scale (and actually
        //  also rotation, even though thats not supported).
        //  This transforms points into this objects coordinate
        //  space.
        Transform _global_transform, _global_transform_no_rotation;
        
        //! [Cached] Global position and size according to
        //  object hierarchy and scale.
        Bounds _global_bounds;
        
        //! [Cached] This variable contains a combination of all the scales
        //  all the way up to the stage for rendering text properly and
        //  calculating its size.
        Vec2 _global_text_scale;
        
        //! Sets the scaling of the object.
        //  For sections this also affects all children.
        GETTER(Vec2, scale);
        
    /**
     * ------------
     * INTERACTIONS
     * ------------
     */
        
        //! The mouse is currently over this object, or one of
        //  its children.
        GETTER(bool, hovered);
        
        //! This object has been selected by clicking on it.
        GETTER(bool, selected);
        
        //! Click has been initiated on top of this object and is
        //  still continously pressed (might not be hovering anymore).
        GETTER(bool, pressed);
        
        //! Indicates whether the object is draggable
        GETTER(bool, draggable);
        callback_handle_t _drag_handle;
        
        //! If set to true, this means that the object interacts with
        //  mouse events. For sections it depends on the current, as
        //  well as descendant objects.
        bool _clickable;
        
        //! Returns the position, relative to object x/y, where dragging
        //  started. Only valid if _dragged == true.
        GETTER(Vec2, relative_drag_start);
        GETTER_I(bool, being_dragged, false);
        
        //! Gives a Z-Index for an item. If this is set > 0, then it will be drawn later than items with smaller z indexes
        int _z_index = 0;
        
    public:
        int z_index() const;
        
    public:
        Drawable(Drawable&) = delete;
        Drawable(Drawable&&) = default;
        
        Drawable(const Type::Class& type);
        Drawable(const Type::Class& type,
                 const Bounds& bounds,
                 const Vec2& origin = Vec2());
        
        virtual ~Drawable();
        Drawable& operator=(const Drawable&) = delete;
        Drawable& operator=(Drawable&&) = default;
        
        /**
         * Add event handling lambda functions.
         */
        void on_hover(const event_handler_yes_t& fn);
        void on_click(const event_handler_yes_t& fn);
        callback_handle_t add_event_handler(EventType type, const event_handler_t& fn); // returns handler-id
        callback_handle_t add_event_handler(EventType type, const event_handler_yes_t& fn);
        callback_handle_t add_event_handler_replace(EventType type, const event_handler_yes_t& fn, const callback_handle_t::element_type*);
        void remove_event_handler(EventType type, const callback_handle_t handler_id);
        void remove_event_handler_raw(EventType type, const callback_handle_t::element_type* handler_id);
        void clear_event_handlers() {
            _event_handlers.clear();
        }
        delete_function_handle_t on_delete(delete_function_t);
        void remove_delete_handler(delete_function_handle_t);
        void clear_delete_handlers() {
            /*for(auto &h : _delete_handlers) {
                if(h)
                    (*h)();
            }*/
            _delete_handlers.clear();
        }
        
        /**
         * Change object properties (only changed if different) and track
         * changes (automatically call set_dirty())
         */
        virtual void set_pos(const Vec2& npos);
        virtual void set_size(const Size2& size);
        virtual void set_bounds(const Bounds& bounds);
        virtual void set_origin(const Vec2& origin);
        virtual void set_rotation(Float2_t radians);
        virtual void set_scale(const Vec2& scale);
        
        void set_scale(Float2_t x, Float2_t y) { set_scale({x, y}); }
        virtual void set_z_index(int index);
        
        //! Accessed by DrawBases to save their own object states (and reuse them).
        CacheObject* cached(const Base* base) const;
        const CacheObject::Ptr& insert_cache(const Base* base, CacheObject::Ptr&& o);
        void remove_cache(const Base* base);
        void clear_cache();

        //! called when the object is hidden / becomes visible
        virtual void on_visibility_change(bool visible);
        void set_is_displayed(bool displayed);
        void set_rendered(bool render);
        
        /// returns true if this object is currently displayed on screen somewhere (i.e. has a parent + the parent has a stage)
        bool is_staged() const;
        
        //! Returns true if the object has been changed for any base or
        //  the specified one
        bool is_dirty(const Base* base = NULL) const;
        
        //! Supposed to return true if x,y (absolute values) is within object boundaries.
        virtual bool in_bounds(Float2_t x, Float2_t y);
        
        virtual Vec2 stage_scale() const;
        
    public:
        /**
         * Functions used in variadic construction of GUI elements.
         */
        virtual void set(Origin origin) { set_origin(origin); }
        virtual void set(Scale scale) { set_scale(scale); }
        virtual void set(Loc loc) { set_pos(loc); }
        virtual void set(Size size) { set_size(size); }
        virtual void set(attr::Box bounds) { set_bounds(bounds); }
        virtual void set(Rotation r) { set_rotation(r); }
        virtual void set(attr::Name name) { set_name(name); }
        virtual void set(attr::ZIndex index) { set_z_index(index); }
        
    protected:
        /**
         * These functions are called for mouse interactions.
         */
        //! Object has been mouse-downed.
        virtual void select();
        //! Other object has been selected.
        virtual void deselect();
        
        //! Mouse-down is repeatedly called, even if the mouse is just
        //  continously pressed but moved.
        virtual void mdown(Float2_t x, Float2_t y, bool left_button);
        virtual void mup(Float2_t x, Float2_t y, bool left_button);
        virtual void scroll(Event e);
        
        virtual bool kdown(Event e);
        virtual bool kup(Event e);
        
        virtual bool text_entered(Event e);
        
        //! Triggered repeatedly while the mouse if within object boundaries (and moved)
        virtual void hover(Event e);
        //virtual void leave(Event e); //! Triggered only once the mouse leaves the object
        
    public:
        //! Sets the parent Section for this element
        virtual void set_parent(SectionInterface* parent);
        
        virtual void clear_parent_dont_check();
        
    public:
        //! If clickable, the object can be clicked, hovered and selected.
        //  Otherwise it will be skipped.
        virtual bool clickable();
        void set_clickable(bool c);
        
        //! checks whether the Drawable is child (or distant child) of
        //  the given other Drawable
        bool is_child_of(Drawable* other) const;
        
        //! Adds event handlers that make the object draggable.
        //  (Also  clickable)
        void set_draggable(bool value = true);
        
        //! Retrieves whether the object is currently being animated,
        //! or in the case of Sections, whether any of its children are animated.
        virtual bool is_animating() noexcept;

        //! Sets whether or not this object is currently animating.
        void set_animating(bool) noexcept;
        
        //! Only returns true if the _current_ object is clickable.
        //! Not overwritten by sections that also respect childrens
        //! clickable status.
        bool immediately_clickable() const noexcept {
            return _clickable;
        }
        
        //! Returns the current rectangle that has been calculated or set
        //  for this object.
        virtual const Bounds& bounds() { return _bounds; }
        
        virtual Vec2 pos() { return bounds().pos(); }
        virtual Size2 size() { return bounds().size(); }
        
        Float2_t width() { return size().width; }
        Float2_t height() { return size().height; }
        
        //! Returns the actual location, incorporating the origin,
        //  but not the global transform
        virtual const Vec2& location() { update_bounds(); return _location; }
        
        //! This will return the transform that, if applied, moves objects to the
        //  absolute, global position of this Drawable. It also scales objects according
        //  to drawing hierarchy.
        const Transform& global_transform();
        const Transform& global_transform_no_rotation();
        
        void global_scale_rotation(Transform &);
        
        //! Same as global_transform, but returns the bounds of the object
        const Bounds& global_bounds();
        
        Transform local_transform();
        Transform local_transform_no_rotation();
        Bounds local_bounds();
        
        virtual std::ostream &operator <<(std::ostream &os);
        
    protected:
        friend class DrawStructure;
        friend class Section;
        friend class SingletonObject;
        friend class Entangled;
        friend class SectionInterface;
        
        //! If swapping fails or objects are incompatible, return false to indicate failure
        virtual bool swap_with(Drawable* d);
        
        //! Returns true if there has been a rotation in this object, or one of its parents. (used for more efficient in_bounds detection)
        virtual bool global_transform(Transform& transform, Transform& no_rotation);
        
        //! Returns true if there was a size/position change since the last
        //  call to update_bounds()
        bool bounds_changed() const { return _bounds_changed; }
        
    public:
        //! Calculates/returns global text scale.
        //  @see _global_text_scale
        const Vec2& global_text_scale();
        
        // to be called if something changed and it needs to be redrawn
        virtual void set_dirty();
        
        //! called whenever there has been a change in size/scale
        virtual void set_bounds_changed();
        
        //! called whenever there is a structural change that requires all parents to update rects and clickable properties
        //  if downwards is true, the propagation will go towards children (if current object is section), as well as upwards (full cycle)
        virtual void structure_changed(bool downwards);
        
        //! updates boundaries / global_transform and clickable (if object is section)
        virtual void update_bounds();
        
        virtual std::string toStr() const;
        static std::string class_name() { return "Drawable"; }
    };
    
    class Rect;
    
    class SectionInterface : public Drawable {
        GETTER(bool, has_children_rect_changed){false};
        
    public:
        SectionInterface(const Type::Class& type, DrawStructure* s)
            : Drawable(type), _background(NULL), _stage(s)
        {}
        SectionInterface(const SectionInterface&) = delete;
        SectionInterface(SectionInterface&&) = default;
        SectionInterface& operator=(SectionInterface&&) = default;
        virtual ~SectionInterface();
        
        void set_pos(const Vec2& pos) override;
        void set_origin(const Vec2& origin) override;
        void set_rotation(Float2_t radians) override;
        void set_size(const Size2& size) override;
        void set_bounds(const Bounds& bounds) override;
        void set_scale(const Vec2& scale) override;

        virtual void on_visibility_change(bool visible) override;
        
        void set_background(const Color& color);
        virtual void set_background(const Color& color, const Color& line);
        
        virtual std::vector<Drawable*>& children() = 0;
        
        virtual void find(Float2_t x, Float2_t y, std::vector<Drawable*>& results);
        virtual Drawable* find(const std::string& search);
        
        virtual bool is_animating() noexcept override;
        
        virtual void set_stage(DrawStructure*);
        virtual Vec2 stage_scale() const override;
        
        std::string toString(const Base* base, const std::string& indent = "");
        virtual std::string toStr() const override;
        
        virtual void before_draw();
        
    public:
        using Drawable::set;
        virtual void set(FillClr clr) { set_background(clr, _bg_line_color); }
        virtual void set(LineClr clr) { set_background(_bg_fill_color, clr); }
        
    protected:
        friend class Drawable;
        friend class Entangled;
        friend class DrawableCollection;
        
        GETTER_PTR(Rect*, background);
        GETTER_PTR(DrawStructure*, stage);
        GETTER_I(Color, bg_fill_color, Transparent);
        GETTER_I(Color, bg_line_color, Transparent);
        
        void update_bounds() override;
        
    public:
        void set_parent(SectionInterface* parent) override;
        void clear_parent_dont_check() override;
        virtual void set_z_index(int) override;
        
    protected:
        virtual void remove_child(Drawable *child) = 0;
        void children_rect_changed();
    };
}

#endif
