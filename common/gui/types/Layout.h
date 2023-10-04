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
        derived_ptr(const std::shared_ptr<Base>& share = nullptr) : ptr(share), raw_ptr(nullptr) {}
        derived_ptr(Base* raw) : ptr(nullptr), raw_ptr(raw) {}
        
        Base& operator*() const { return ptr ? *ptr : *raw_ptr; }
        Base* get() const { return ptr ? ptr.get() : raw_ptr; }
        template<typename T> T* to() const { auto ptr = dynamic_cast<T*>(get()); if(!ptr) throw U_EXCEPTION("Cannot cast object to specified type."); return ptr; }
        
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
        GETTER(std::vector<Ptr>, objects)
        
    public:
        template<typename T, typename... Args>
        static Layout::Ptr Make(Args&&... args) {
            return Layout::Ptr(std::make_shared<T>(std::forward<Args>(args)...));
        }
        
    public:
        Layout(const std::vector<Layout::Ptr>& = {});
        virtual ~Layout() { clear_children(); }
        
        void update() override;
        void add_child(size_t pos, Layout::Ptr ptr);
        void add_child(Layout::Ptr ptr);
        
        void remove_child(Layout::Ptr ptr);
        void remove_child(Drawable* ptr) override;
        void set_children(const std::vector<Layout::Ptr>&);
        void clear_children() override;
        
        
        virtual void update_layout();
        virtual void auto_size(Margin margins) override;
    };
    
    class HorizontalLayout : public Layout {
    public:
        enum Policy {
            CENTER, TOP, BOTTOM
        };
        
    protected:
        GETTER(Bounds, margins)
        GETTER(Policy, policy)
        
    public:
        HorizontalLayout(const Bounds& margins)
            : HorizontalLayout({}, Vec2(), margins)
        {}
        
        HorizontalLayout(const std::vector<Layout::Ptr>& objects = {},
                         const Vec2& position = Vec2(),
                         const Bounds& margins = {5, 5, 5, 5});
        
        void set_policy(Policy);
        void set_margins(const Bounds&);
        virtual std::string name() const override { return "HorizontalLayout"; }
        
        using Layout::set;
        void set(attr::Margins margins) {
            set_margins(margins);
        }
        
        void update_layout() override;
    };
    
    class VerticalLayout : public Layout {
    public:
        enum Policy {
            CENTER, LEFT, RIGHT
        };
        
    protected:
        GETTER(Bounds, margins)
        GETTER(Policy, policy)
        
    public:
        VerticalLayout(const Bounds& margins)
            : VerticalLayout({}, Vec2(), margins)
        {}
        VerticalLayout(const std::vector<Layout::Ptr>& objects = {},
                         const Vec2& position = Vec2(),
                         const Bounds& margins = {5, 5, 5, 5});
        
        void set_policy(Policy);
        void set_margins(const Bounds&);
        virtual std::string name() const override { return "VerticalLayout"; }
        
        using Layout::set;
        void set(attr::Margins margins) {
            set_margins(margins);
        }
        
        void update_layout() override;
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
    enum Policy {
        CENTER, LEFT, RIGHT
    };
    
protected:
    struct Settings {
        attr::VerticalClr verticalClr{DarkGray.alpha(125)};
        attr::HorizontalClr horizontalClr{DarkCyan.alpha(125)};
        attr::Margins margins{5,5,5,5};
        Policy policy{Policy::LEFT};
        
    } _settings;
    
    GETTER(GridInfo, grid_info)
    
    Vec2 _last_hover;
    
    std::shared_ptr<Rect> _vertical_rect;
    std::shared_ptr<Rect> _horizontal_rect;
    
public:
    template<typename... Args>
    GridLayout(Args... args)
    {
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
    void auto_size(Margin margins) override;
    void children_rect_changed() override;
    void update() override;
    
    auto& margins() const { return _settings.margins; }
    auto& vertical_clr() const { return _settings.verticalClr; }
    auto& horizontal_clr() const { return _settings.horizontalClr; }
    
    using Layout::set;
    void set(Policy);
    void set(attr::Margins);
    void set(attr::VerticalClr);
    void set(attr::HorizontalClr);
    void set(const std::vector<Layout::Ptr>& objects);
    
    virtual std::string name() const override { return "GridLayout"; }
    
    void update_layout() override;
    
private:
    void update_hover();
};

}
