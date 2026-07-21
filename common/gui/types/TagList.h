#pragma once

#include <commons.pc.h>
#include <gui/ListAttributes.h>
#include <gui/types/Dropdown.h>
#include <gui/types/Layout.h>

namespace cmn::gui {

class TagList : public Entangled {
public:
    NUMBER_ALIAS(AllowNew_t, bool)
    NUMBER_ALIAS(MatchThreshold_t, float)

    using AddCallback = std::function<void(const std::string&)>;
    using RemoveCallback = std::function<void(size_t, const std::string&)>;

private:
    GETTER(std::vector<std::string>, tags);
    GETTER(std::vector<std::string>, catalog);
    GETTER_I(bool, allow_new, true);
    GETTER_I(float, match_threshold, 1.f);

    AddCallback _on_add;
    RemoveCallback _on_remove;

    derived_ptr<FloatingLayout> _flow{new FloatingLayout};
    derived_ptr<Dropdown> _dropdown{new Dropdown};
    std::vector<Layout::Ptr> _flow_objects;

    Font _item_font{0.65f};
    Vec2 _item_padding{8, 4};
    Color _item_text_color{White};
    Color _item_line_color{Transparent};
    CornerFlags_t _item_corners{CornerFlags::Rounded(8)};
    std::optional<Color> _item_fill_color;

    Size2 _input_size{150, 28};
    Size2 _input_list_size{220, 180};
    std::string _input_placeholder_text{"Add tag..."};
    Font _input_font{0.65f};
    Color _input_fill_color{White.alpha(210)};
    Color _input_line_color{White.alpha(50)};
    Color _input_text_color{Black};

public:
    template<typename... Args>
    TagList(Args... args) {
        init();
        (set(std::forward<Args>(args)), ...);
    }

    TagList(TagList&&) = delete;
    TagList& operator=(TagList&&) = delete;

    using Entangled::set;
    void set(const std::vector<std::string>& values) { set_tags(values); }
    void set(attr::SizeLimit limit);
    void set(attr::Margins margins);
    void set(OuterPadding padding);
    void set(FloatingLayout::Policy policy);
    void set(AllowNew_t value) { set_allow_new(value); }
    void set(MatchThreshold_t value) { set_match_threshold(value); }

    void set_tags(const std::vector<std::string>& values);
    void set_tags(std::vector<std::string>&& values);
    void set_catalog(const std::vector<std::string>& values);
    void set_catalog(std::vector<std::string>&& values);
    void set_allow_new(bool value);
    void set_match_threshold(float value);

    void on_add(AddCallback callback);
    void on_remove(RemoveCallback callback);
    void request_remove(size_t index);

    void set(ItemFont_t value);
    void set(ItemPadding_t value);
    void set(ItemFillClr_t value);
    void set(ListFillClr_t value);
    void set(std::optional<ItemFillClr_t> value);
    void set(ItemLineColor_t value);
    void set(ItemTextClr_t value);
    void set(CornerFlags_t value) override;

    void set(LabelDims_t value);
    void set(ListDims_t value);
    void set(Placeholder_t value);
    void set(LabelFont_t value);
    void set(LabelFillClr_t value);
    void set(LabelLineColor_t value);
    void set(LabelColor_t value);

    FloatingLayout& flow() { return *_flow; }
    const FloatingLayout& flow() const { return *_flow; }
    Dropdown& input() { return *_dropdown; }
    const Dropdown& input() const { return *_dropdown; }

    void update() override;
    std::string name() const override { return "TagList"; }

private:
    void init();
    void mark_structure_dirty();
    void rebuild_structure();
    void refresh_catalog();
    void position_input();
    void commit_typed_text();
    void request_add(std::string value, bool explicit_selection);
    static std::vector<std::string> normalize_values(std::vector<std::string> values);
    static float similarity(std::string_view lhs, std::string_view rhs);
    static bool equivalent(std::string_view lhs, std::string_view rhs);
    Color tag_color(std::string_view value);
};

}
