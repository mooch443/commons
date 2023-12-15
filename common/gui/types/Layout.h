#pragma once

#include <gui/types/Entangled.h>

namespace gui {
    template<class Base>
    class derived_ptr {
    public:
        std::shared_ptr<Base> ptr;
        Base* raw_ptr;

        template<typename T>
            requires std::is_base_of_v<Base, T>
        derived_ptr(const derived_ptr<T>& share)
            : ptr(share.ptr), raw_ptr(share.raw_ptr)
        {}
        template<typename T>
            requires std::is_base_of_v<Base, T>
        derived_ptr(derived_ptr<T>&& share)
            : ptr(std::move(share.ptr)), raw_ptr(std::move(share.raw_ptr))
        {}
        derived_ptr(const std::shared_ptr<Base>& share = nullptr) : ptr(share), raw_ptr(nullptr) {}
        derived_ptr(Base* raw) : ptr(nullptr), raw_ptr(raw) {}
        
        Base& operator*() const { return ptr ? *ptr : *raw_ptr; }
        Base* get() const { return ptr ? ptr.get() : raw_ptr; }
        template<typename T> T* to() const {
            auto ptr = dynamic_cast<T*>(get());
            if(!ptr)
                throw U_EXCEPTION("Cannot cast object to specified type.");
            return ptr;
        }
        
        template<typename T>
        constexpr bool is() const { return dynamic_cast<T*>(get()); }
        
        bool operator==(Base* raw) const { return get() == raw; }
        //bool operator==(decltype(ptr) other) const { return ptr == other; }
        bool operator==(derived_ptr<Base> other) const { return get() == other.get(); }
        bool operator<(Base* raw) const { return get() < raw; }
        bool operator<(derived_ptr<Base> other) const { return get() < other.get(); }
        
        operator bool() const { return get() != nullptr; }
        Base* operator ->() const { return get(); }
        
        template<typename T>
        operator derived_ptr<T> () {
            if(ptr)
                return derived_ptr<T>(std::static_pointer_cast<T>(ptr));
            return derived_ptr<T>(static_cast<T*>(raw_ptr));
        }
    };
    
    class Layout : public Entangled {
    public:
        typedef derived_ptr<Drawable> Ptr;
        
    private:
        GETTER_NCONST(std::vector<Ptr>, objects)
        
    protected:
        GETTER(attr::Margins, margins);
        
    public:
        template<typename T, typename... Args>
        static Layout::Ptr Make(Args&&... args) {
            return Layout::Ptr(std::make_shared<T>(std::forward<Args>(args)...));
        }
        
    public:
        template<typename... Args>
        Layout(Args... args)
        {
            create(std::forward<Args>(args)...);
        }
        
        Layout(Layout&&) noexcept = default;
        Layout& operator=(Layout&&) noexcept = default;
        
        template<typename... Args>
        void create(Args... args) {
            (set(std::forward<Args>(args)), ...);
            init();
        }
        
    private:
        void init();
        
    public:
        virtual ~Layout() { clear_children(); }
        
        void update() override;
        void add_child(size_t pos, Layout::Ptr ptr);
        void add_child(Layout::Ptr ptr);
        void replace_child(size_t pos, Layout::Ptr ptr);
        
        void remove_child(Layout::Ptr ptr);
        void remove_child(Drawable* ptr) override;
        void set_children(const std::vector<Layout::Ptr>&);
        void set_children(std::vector<Layout::Ptr>&&);
        void clear_children() override;
        
        using Entangled::set;
        void set(attr::Margins margins) {
            set_margins(margins);
        }
        void set(const std::vector<Layout::Ptr>& ptr) {
            set_children(ptr);
        }
        void set(std::vector<Layout::Ptr>&& ptr) {
            set_children(std::move(ptr));
        }
        void set_margins(const attr::Margins&);
        virtual void update_layout();
        virtual void auto_size();
        void set_parent(SectionInterface*) override;
        void set_content_changed(bool) override;
        
    protected:
        using Entangled::auto_size;
        static void _apply_to_children(Drawable* ptr, const std::function<void(Drawable*)>&);
        void apply_to_children(const std::function<void(Drawable*)>&);
    };

    class PlaceinLayout : public Layout {
    public:
        // Constructors and destructors, if needed
        PlaceinLayout(const std::vector<Layout::Ptr>& vec = {}) : Layout(vec) {}
        virtual ~PlaceinLayout() { /* custom destruction logic if needed */ }
        
        PlaceinLayout(PlaceinLayout&&) noexcept = default;
        PlaceinLayout& operator=(PlaceinLayout&&) noexcept = default;

        // Use the using declaration to make base class member functions visible
        using Layout::update;
        using Layout::add_child;
        using Layout::remove_child;
        using Layout::set_children;
        using Layout::clear_children;
        
        using Layout::set;
        
        void update_layout() override {
            auto_size();
        }
        void auto_size() override {}
    };

    
    class HorizontalLayout : public Layout {
    public:
        enum Policy {
            CENTER, TOP, BOTTOM
        };
        
    protected:
        GETTER(Policy, policy);
        
    public:
        template<typename... Args>
        HorizontalLayout(Args... args)
        {
            create(std::forward<Args>(args)...);
        }
        
        HorizontalLayout(HorizontalLayout&&) noexcept = default;
        HorizontalLayout& operator=(HorizontalLayout&&) noexcept = default;
        
        template<typename... Args>
        void create(Args... args) {
            _policy = Policy::CENTER;
            (set(std::forward<Args>(args)), ...);
            init();
        }
        
    private:
        void init();
        
    public:
        using Layout::set;
        void set(Policy p) { set_policy(p); }
        void set_policy(Policy);
        virtual std::string name() const override { return "HorizontalLayout"; }
        
        void update_layout() override;
        void auto_size() override;
    };
    
    class VerticalLayout : public Layout {
    public:
        enum Policy {
            CENTER, LEFT, RIGHT
        };
        
    protected:
        GETTER(Policy, policy);
        
    public:
        template<typename... Args>
        VerticalLayout(Args... args)
        {
            create(std::forward<Args>(args)...);
        }
        
        VerticalLayout(VerticalLayout&&) noexcept = default;
        VerticalLayout& operator=(VerticalLayout&&) noexcept = default;
        
        template<typename... Args>
        void create(Args... args) {
            _policy = Policy::LEFT;
            (set(std::forward<Args>(args)), ...);
            init();
        }
        
    private:
        void init();
        
    public:
        using Layout::set;
        void set_policy(Policy);
        void set(Policy p) { set_policy(p); }
        
        void update_layout() override;
        void auto_size() override;
    };

class GridInfo {
public:
    using GridBounds = std::vector<std::vector<Bounds>>;
    GridBounds gridBounds;
    size_t numRows;
    size_t numCols;

    GridInfo() : numRows(0), numCols(0) {}

    // Initialize with the given grid bounds
    GridInfo(GridBounds bounds) : gridBounds(bounds) {
        numRows = bounds.size();
        numCols = numRows > 0 ? bounds[0].size() : 0;
    }

    // Safe access to individual cell bounds
    Bounds getCellBounds(size_t row, size_t col) const {
        if (row >= gridBounds.size() || col >= gridBounds[row].size()) {
            throw std::out_of_range("Invalid row or column index");
        }
        return gridBounds[row][col];
    }

    // Returns the number of rows
    size_t rowCount() const {
        return numRows;
    }

    // Returns the number of columns
    size_t colCount() const {
        return numCols;
    }
    
    std::string toStr() const;
    static std::string class_name() { return "GridInfo"; }
};

class GridLayout : public Layout {
public:
    enum class HPolicy {
        CENTER, LEFT, RIGHT
    };
    enum class VPolicy {
        CENTER, TOP, BOTTOM
    };
    
protected:
    struct Settings {
        attr::VerticalClr verticalClr{DarkGray.alpha(125)};
        attr::HorizontalClr horizontalClr{DarkCyan.alpha(125)};
        HPolicy hpolicy{HPolicy::LEFT};
        VPolicy vpolicy{VPolicy::TOP};
        
    } _settings;
    
    GETTER(GridInfo, grid_info);
    
    Vec2 _last_hover;
    
    std::shared_ptr<Rect> _vertical_rect;
    std::shared_ptr<Rect> _horizontal_rect;
    
public:
    template<typename... Args>
    GridLayout(Args... args)
    {
        _vertical_rect = std::make_shared<Rect>();
        _horizontal_rect = std::make_shared<Rect>();
        
        create(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void create(Args... args) {
        (set(std::forward<Args>(args)), ...);
        init();
    }
    
private:
    void init();
    
public:
    void auto_size() override;
    void children_rect_changed() override;
    void update() override;
    
    auto& vertical_clr() const { return _settings.verticalClr; }
    auto& horizontal_clr() const { return _settings.horizontalClr; }
    
    using Layout::set;
    void set(HPolicy);
    void set(VPolicy);
    void set(attr::VerticalClr);
    void set(attr::HorizontalClr);
    void set(const std::vector<Layout::Ptr>& objects);
    
    virtual std::string name() const override { return "GridLayout"; }
    
    void update_layout() override;
    
private:
    void update_hover();
};

}
