#ifndef _GRAPH_H
#define _GRAPH_H

#include <commons.pc.h>
#include <misc/colors.h>
#include <gui/DrawStructure.h>

namespace cmn::gui {
    /*class Graph;
    class GuiGraph : public Entangled {
        GETTER(float, alpha);
        
    public:
        GuiGraph() : _alpha(0.75) {
            
        }
        
        void set_alpha(float alpha) {
            if(_alpha == alpha)
                return;
            
            _alpha = alpha;
            set_background(Black.alpha(175 * _alpha));
            set_dirty();
        }
        
        void set_graph(const Graph& graph);
    };*/
    
    class Graph : public Entangled {
        static ColorWheel _colors;
        static Font title_font;
        
    public:
        //GETTER_NCONST(GuiGraph, gui_obj);
        
    public:
        enum Type {
            CONTINUOUS = 1, DISCRETE = 2, POINTS = 4, AREA = 8
        };
        
        struct Points {
            std::function<void(const std::string&, float)> _hover_fn;
            std::vector<Vec2> points;
        };
        
        struct Function {
            friend class GuiGraph;
            
            const gui::Color _color;
            const std::function<double(double x)> _get_y;
            const int _type;
            const std::string _name;
            const std::string _unit_name;
            std::shared_ptr<Points> _points;
            
            Function(const std::string& name, int type, decltype(_get_y) get_y, gui::Color clr = gui::Color(), const std::string unit_name = "")
                : _color(gui::Color() == clr ? color_for_function(name) : clr),
                _get_y(get_y), _type(type), _name(name), _unit_name(unit_name)
            { }
            
            double operator()(double x) const {
                return _get_y(x);
            }
            
        private:
            static auto& fn_clrs_mutex() {
                static auto _fn_clrs_mutex = new LOGGED_MUTEX("Graph::Function::fn_clrs_mutex");
                return *_fn_clrs_mutex;
            }
            static auto& fn_clrs() {
                static std::unordered_map<std::string, gui::Color> _fn_clrs;
                return _fn_clrs;
            }
            
        public:
            static gui::Color color_for_function(const std::string& name);
        };
        
        typedef std::function<void(cv::Mat&)> draw_function;
        
        struct GLine {
            std::vector<Vec2> v;
            Color color;
            float thickness;
        };
        
    private:
        std::string _name;
        GETTER(std::vector<Function>, functions);
        std::unordered_map<std::string, std::vector<std::shared_ptr<Circle>>> _gui_points;
        std::vector<GLine> _lines;
        
        GETTER(Vec2, margin);
        //GETTER(cv::Size, size);
        GETTER(Rangef, x_range);
        GETTER(Rangef, y_range);
        GETTER(float, zero);
        char _xyaxis;
        
        std::vector<std::shared_ptr<Text>> _labels;
        std::string _title_text;
        Text _title;
        
        std::shared_ptr<Circle> _last_hovered_circle;
        
    public:
        Graph(const Bounds& bounds, const std::string& name, const Rangef& x_range = Rangef(-FLT_MAX, FLT_MAX), const Rangef& y_range = Rangef(-FLT_MAX, FLT_MAX));
        virtual ~Graph() {}
        
        std::string name() const override { return _name; }
        
        Function add_function(const Function& function);
        void add_points(const std::string& name, const std::vector<Vec2>& points, Color color = Transparent);
        void add_points(const std::string& name, const std::vector<Vec2>& points, std::function<void(const std::string&, float)> on_hover, Color color = Transparent);
        void add_line(Vec2 pos0, Vec2 pos1, Color color, float thickness = 1);
        void add_line(const std::vector<Vec2>& v, Color color, float thickness = 1);
        
        void set_axis(bool xaxis, bool yaxis);
        void set_title(const std::string& str);
        virtual void update() override;
        //void display(gui::DrawStructure& base, const Vec2& pos = Vec2(0, 0), float scale = 1.0, float transparency = 1.0);
        bool empty() const { return _functions.empty(); }
        void clear();
        
        void set_margin(const Vec2& margin);
        
        void set_zero(float zero) { _zero = zero; set_content_changed(true); }
        void set_ranges(const Rangef& x_range, const Rangef& y_range);
        
        void export_data(const std::string& filename, std::function<void(float)> *percent_callback = NULL) const;
        void save_npz(const std::string& filename, std::function<void(float)> *percent_callback = NULL, bool quiet=false) const;
        
        void highlight_point(const std::string& name, float x);
        Vec2 transform_point(Vec2 point);
        
    private:
        void highlight_point(std::shared_ptr<Circle> circle);
        void update_title();
    };
}

#endif
