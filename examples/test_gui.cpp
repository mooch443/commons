#include <gui/IMGUIBase.h>

int main() {
    using namespace gui;
    
    constexpr char owl[] {
        " * * * * * * * * *"
        "  *     * *     * "
        " *   *   *   *   *"
        "    * *     * *   "
        " *   *       *   *"
        "  *     * *     * "
        " * *     *     * *"
        "  * *         *   "
        " * * * * * * *   *"
        "  * * * *         "
        " * * * * *       *"
        "  * * * *         "
        "   * * * *       *"
        "    * * * *       "
        "     * * * *     *"
        "      * * * *     "
        "       * * * *   *"
        "        *   * *   "
        "       *   *   * *"
        "  * * * * * *   * "
        "                 *"
    };
    
    Vec2 offset[sizeof(owl)] = {500};
    
    DrawStructure graph(1024,1024);
    Timer timer;
    
    IMGUIBase base("Test", graph, [&]() -> bool {
        const float radius = 5;
        
        float dt = timer.elapsed();
        auto target = graph.mouse_position();
        
        
        Vec2 start{0, 0};
        for (size_t i=0; i<sizeof(owl); ++i) {
            if(i%18 == 0) {
                start.y += radius * 2;
                start.x = 0;
            }
            
            float f = start.length() / 300;
            if(owl[i] == '*')
                graph.circle(offset[i], radius * (1 - f) + 3, White.alpha(200), Color(255, 125 * f, 125 * (1 - f)));
            
            offset[i] += (target + start - 18 * radius - offset[i]) * dt * ((target + start - 18 * radius - offset[i]).length() / 100 + 1) * ((start  - 18 * radius).length() / 100 + 1);
            start.x += radius * 2;
        }
        
        timer.reset();
        
        return true;
        
    }, [](const Event&) {
        
    });
    base.loop();
}
