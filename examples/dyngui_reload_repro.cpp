#include <commons.pc.h>
#include <gui/types/Combobox.h>
#include <misc/GlobalSettings.h>

using namespace cmn;
using namespace cmn::gui;

int main() {
    if(!GlobalSettings::has_value("source")) {
        FormatError("Missing expected setting 'source'.");
        return 2;
    }

    derived_ptr<Combobox> combo{new Combobox(nullptr)};

    constexpr size_t iterations = 5000;
    for(size_t i = 0; i < iterations; ++i) {
        auto root = Layout::Make<VerticalLayout>(std::vector<Layout::Ptr>{combo});
        root->before_draw();

        combo->set(ParmName{"source"});
        combo->before_draw();
        root->before_draw();

        root = nullptr;

        combo->set(ParmName{});
        combo->before_draw();

        if((i + 1) % 250 == 0) {
            Print("Completed iteration ", i + 1, "/", iterations, ".");
        }
    }

    combo = nullptr;
    Print("Completed without a crash.");
    return 0;
}
