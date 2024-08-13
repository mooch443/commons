#include "Table.h"

namespace cmn::gui {
    Table::Table()
        : _default_font(0.75)
    {
        
    }
    
    void Table::update() {
        if(!content_changed())
            return;
        
        auto ctx = OpenContext();
        using column_t = size_t;
        //using row_t = size_t;
        
        std::map<column_t, Bounds> title_bounds;
        for(const auto & [index, title] : _columns) {
            title_bounds[index] = add<Text>(Str{title}, TextClr{White}, Font(_default_font.size * 1.25, Style::Bold))->bounds();
        }
    }
    
    void Table::add_column(Column ) {
        
    }
    
    void Table::add_row(const Row& ) {
        
    }
}
