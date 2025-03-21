#pragma once

#include <commons.pc.h>
#include <gui/types/Entangled.h>
#include <misc/derived_ptr.h>

namespace cmn::gui {
    ATTRIBUTE_ALIAS(MinSize, Size2);

    class Layout : public Entangled {
    public:
        typedef derived_ptr<Drawable> Ptr;
        
    private:
        GETTER_NCONST(std::vector<Ptr>, objects);
        bool _layout_dirty{true};
        
    protected:
        GETTER(attr::Margins, margins);
        GETTER(MinSize, minSize);
        
    public:
        template<typename T, typename... Args>
        static Layout::Ptr Make(Args&&... args) {
            return Layout::Ptr(new T(std::forward<Args>(args)...));
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
            _minSize = MinSize{};
            _margins = Margins{};
            
            (set(std::forward<Args>(args)), ...);
            init();
        }
        
    protected:
        void init();
        
    public:
        virtual ~Layout();
        
        void update() override;
        void add_child(size_t pos, Layout::Ptr ptr);
        void add_child(Layout::Ptr ptr);
        bool replace_child(size_t pos, Layout::Ptr ptr);
        
        void remove_child(const Layout::Ptr& ptr);
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
        void set(MinSize);
        void set_margins(const attr::Margins&);
        void update_layout();
        virtual void auto_size();
        void set_parent(SectionInterface*) override;
        void set_content_changed(bool) override;
        void set_layout_dirty();
        
        void set_size(const Size2&) override;
        void set_pos(const Vec2&) override;
        void set_bounds(const Bounds&) override;
        void set_bounds_changed() override;
        
        void set_stage(gui::DrawStructure*) override;
        
    protected:
        using Entangled::auto_size;
        virtual void _update_layout();
        static void _apply_to_children(Drawable* ptr, const std::function<void(Drawable*)>&);
        void apply_to_children(const std::function<void(Drawable*)>&);
    };

    class PlaceinLayout : public Layout {
    public:
        PlaceinLayout(const std::vector<Layout::Ptr>& vec = {}) : Layout(vec) {}
        virtual ~PlaceinLayout() { }
        
        PlaceinLayout(PlaceinLayout&&) noexcept = default;
        PlaceinLayout& operator=(PlaceinLayout&&) noexcept = default;

        // Use the using declaration to make base class member functions visible
        using Layout::update;
        using Layout::add_child;
        using Layout::remove_child;
        using Layout::set_children;
        using Layout::clear_children;
        
        using Layout::set;
        
        //void auto_size() override {}
        
    /*protected:
        void _update_layout() override {
            auto_size();
        }*/
    };

    
    class HorizontalLayout : public Layout {
    public:
        enum Policy {
            CENTER, TOP, BOTTOM
        };
        
    protected:
        GETTER(Policy, policy){Policy::CENTER};
        
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
        void auto_size() override;
        
    protected:
        void _update_layout() override;
    };
    
    class VerticalLayout : public Layout {
    public:
        enum Policy {
            CENTER, LEFT, RIGHT
        };
        
    protected:
        GETTER(Policy, policy){Policy::LEFT};
        
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
        void auto_size() override;
        
    protected:
        void _update_layout() override;
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
    
    bool hasCell(size_t row, size_t col) const {
        if (row >= gridBounds.size() || col >= gridBounds[row].size()) {
            return false;
        }
        return true;
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

NUMBER_ALIAS(CellFillInterval, uint16_t);
ATTRIBUTE_ALIAS(MinCellSize, Size2);

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
        attr::CellFillClr cellFillClr{Transparent};
        attr::CellLineClr cellLineClr{Transparent};
        CellFillInterval cellFillInterval{1};
        HPolicy hpolicy{HPolicy::LEFT};
        VPolicy vpolicy{VPolicy::TOP};
        MinCellSize minCellSize;
        
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
        _settings = {};
        
        (set(std::forward<Args>(args)), ...);
        init();
    }
    
private:
    void init();
    
public:
    void auto_size() override;
    void update() override;
    
    auto& vertical_clr() const { return _settings.verticalClr; }
    auto& horizontal_clr() const { return _settings.horizontalClr; }
    
    using Layout::set;
    void set(HPolicy);
    void set(VPolicy);
    void set(attr::VerticalClr);
    void set(attr::HorizontalClr);
    void set(attr::CellFillClr);
    void set(attr::CellLineClr);
    void set(CellFillInterval);
    void set(MinCellSize);
    void set(const std::vector<Layout::Ptr>& objects);
    
    virtual std::string name() const override { return "GridLayout"; }
    
private:
    void update_hover();
    void _update_layout() override;
    void set_parent(SectionInterface*) override;
};

}
