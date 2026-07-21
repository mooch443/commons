#include "TagList.h"
#include <gui/types/Dropdown.h>

namespace cmn::gui {

namespace {

std::string trimmed(std::string_view value) {
    return std::string(utils::trim(value));
}

}

void TagList::init() {
    _flow->set(FloatingLayout::Policy::HorizontalFirst);
    _flow->set(attr::Margins{2, 2, 2, 2});

    _dropdown->set(attr::Box{0, 0, _input_size.width, _input_size.height});
    _dropdown->set(ListDims_t{_input_list_size});
    _dropdown->set(Font{_input_font});
    _dropdown->list().set(ItemFont_t{_input_font});
    _dropdown->set(FillClr{_input_fill_color});
    _dropdown->set(ListFillClr_t{_input_fill_color});
    _dropdown->set(LineClr{_input_line_color});
    _dropdown->set(ListLineClr_t{_input_line_color});
    _dropdown->set(TextClr{_input_text_color});
    _dropdown->textfield()->set_placeholder(_input_placeholder_text);
    _dropdown->set(ZIndex{3});

    _dropdown->on_select([this](Dropdown::RawIndex, const Dropdown::TextItem& item) {
        request_add(item.name(), true);
    });
    _dropdown->set(Textfield::OnEnter_t{[this] {
        if(auto selected = _dropdown->currently_hovered_item(); selected.has_value())
            request_add(selected->name(), true);
        else
            commit_typed_text();
    }});

    set_clickable(true);
}

std::vector<std::string> TagList::normalize_values(std::vector<std::string> values) {
    std::vector<std::string> normalized;
    normalized.reserve(values.size());
    for(auto& value : values) {
        value = trimmed(value);
        if(value.empty() || std::find(normalized.begin(), normalized.end(), value) != normalized.end())
            continue;
        normalized.emplace_back(std::move(value));
    }
    return normalized;
}

void TagList::set_tags(const std::vector<std::string>& values) {
    set_tags(std::vector<std::string>(values));
}

void TagList::set_tags(std::vector<std::string>&& values) {
    auto normalized = normalize_values(std::move(values));
    if(_tags == normalized)
        return;
    _tags = std::move(normalized);
    refresh_catalog();
    mark_structure_dirty();
}

void TagList::set_catalog(const std::vector<std::string>& values) {
    set_catalog(std::vector<std::string>(values));
}

void TagList::set_catalog(std::vector<std::string>&& values) {
    auto normalized = normalize_values(std::move(values));
    if(_catalog == normalized)
        return;
    _catalog = std::move(normalized);
    refresh_catalog();
}

void TagList::set_allow_new(bool value) {
    if(_allow_new == value)
        return;
    _allow_new = value;
    set_content_changed(true);
}

void TagList::set_match_threshold(float value) {
    value = saturate(value, 0.f, 1.f);
    if(_match_threshold == value)
        return;
    _match_threshold = value;
}

void TagList::on_add(AddCallback callback) {
    const bool had_callback = bool(_on_add);
    _on_add = std::move(callback);
    if(had_callback != bool(_on_add))
        mark_structure_dirty();
}

void TagList::on_remove(RemoveCallback callback) {
    const bool had_callback = bool(_on_remove);
    _on_remove = std::move(callback);
    if(had_callback != bool(_on_remove))
        mark_structure_dirty();
}

void TagList::request_remove(size_t index) {
    if(not _on_remove || index >= _tags.size())
        return;
    const auto tag = _tags[index];
    _on_remove(index, tag);
}

void TagList::set(attr::SizeLimit limit) {
    auto new_value = attr::SizeLimit{limit.width, max(0, limit.height - 10 - _input_size.height)};
    if(new_value != _flow->max_size()) {
        _flow->set(new_value);
        mark_structure_dirty();
    }
}

void TagList::set(attr::Margins margins) {
    _flow->set(margins);
}

void TagList::set(OuterPadding padding) {
    _flow->set(padding);
}

void TagList::set(FloatingLayout::Policy policy) {
    _flow->set(policy);
}

void TagList::set(ItemFont_t value) {
    if(_item_font == value)
        return;
    _item_font = value;
    mark_structure_dirty();
}

void TagList::set(ItemPadding_t value) {
    if(_item_padding == value)
        return;
    _item_padding = value;
    mark_structure_dirty();
}

void TagList::set(ItemFillClr_t value) {
    set(std::optional<ItemFillClr_t>{value});
}

void TagList::set(ListFillClr_t value) {
    if(_dropdown)
        _dropdown->set(value);
}

void TagList::set(std::optional<ItemFillClr_t> value) {
    std::optional<Color> color;
    if(value)
        color = static_cast<const Color&>(*value);
    if(_item_fill_color == color)
        return;
    _item_fill_color = color;
    mark_structure_dirty();
}

void TagList::set(ItemLineColor_t value) {
    if(_item_line_color == value)
        return;
    _item_line_color = value;
    mark_structure_dirty();
}

void TagList::set(ItemTextClr_t value) {
    if(_item_text_color == value)
        return;
    _item_text_color = value;
    mark_structure_dirty();
}

void TagList::set(CornerFlags_t value) {
    if(_item_corners == value)
        return;
    _item_corners = value;
    mark_structure_dirty();
}

void TagList::set(LabelDims_t value) {
    if(_input_size == value)
        return;
    _input_size = value;
    _dropdown->set_size(value);
    mark_structure_dirty();
}

void TagList::set(ListDims_t value) {
    if(_input_list_size == value)
        return;
    _input_list_size = value;
    _dropdown->set(value);
}

void TagList::set(Placeholder_t value) {
    if(_input_placeholder_text == value)
        return;
    _input_placeholder_text = std::move(value);
    _dropdown->textfield()->set_placeholder(_input_placeholder_text);
}

void TagList::set(LabelFont_t value) {
    if(_input_font == value)
        return;
    _input_font = value;
    const Font font = value;
    _dropdown->set(font);
    _dropdown->list().set(ItemFont_t{font});
}

void TagList::set(LabelFillClr_t value) {
    if(_input_fill_color == value)
        return;
    _input_fill_color = value;
    const Color color = value;
    _dropdown->set(FillClr{color});
    _dropdown->set(ListFillClr_t{color});
}

void TagList::set(LabelLineColor_t value) {
    if(_input_line_color == value)
        return;
    _input_line_color = value;
    const Color color = value;
    _dropdown->set(LineClr{color});
    _dropdown->set(ListLineClr_t{color});
}

void TagList::set(LabelColor_t value) {
    if(_input_text_color == value)
        return;
    _input_text_color = value;
    _dropdown->set(TextClr{static_cast<const Color&>(value)});
}

void TagList::mark_structure_dirty() {
    set_content_changed(true);
}

Color TagList::tag_color(std::string_view value) {
    if(not _catalog.empty()) {
        auto it = std::find(_catalog.begin(), _catalog.end(), value);
        auto index = std::distance(_catalog.begin(), it);
        
        ColorWheel wheel((uint32_t)saturate(index, 0, (int)_catalog.size()));
        return wheel.next().saturation(0.65f).exposure(0.72f).alpha(220);
    }
    
    auto hash = std::hash<std::string_view>{}(value);
    /*uint32_t hash = 2166136261u;
    for(const auto c : value) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }*/
    ColorWheel wheel(hash);
    return wheel.next().saturation(0.65f).exposure(0.72f).alpha(220);
}

void TagList::rebuild_structure() {
    if(not content_changed())
        return;
    
    //_flow_objects.clear();
    _flow->set_layout_dirty();
    _flow_objects.reserve(_tags.size());

    for(size_t index = 0; index < _tags.size(); ++index) {
        const auto& tag = _tags[index];
        HorizontalLayout* chip{nullptr};
        
        auto add_remove = [this, &chip, index](){
            auto remove = Layout::Make<StaticText>{
                Str{"<sym>✕</sym>"},
                TextClr{_item_text_color.alpha(190)},
                _item_font,
                attr::Margins{
                    0,
                    _item_padding.y,
                    _item_padding.x,
                    _item_padding.y
                },
                Clickable{true}
            }();
            remove->on_hover([weak = std::weak_ptr<StaticText>(remove.get_smart()), color = _item_text_color](Event event) {
                if(auto ptr = weak.lock())
                    ptr->set(TextClr{color.alpha(event.hover.hovered ? 255 : 50)});
            });
            remove->on_click([this, index](Event) {
                request_remove(index);
            });
        
            chip->add_child(std::move(remove));
        };
        
        if(index >= _flow_objects.size()) {
            auto ptr = Layout::Make<HorizontalLayout>{
                std::vector<Layout::Ptr>{
                    Layout::Make<StaticText>{
                        Str{tag},
                        TextClr{_item_text_color},
                        SizeLimit{_flow->max_size().width - _item_padding.x * 2 - (_on_remove ? 30 : 0), 0},
                        attr::Margins{
                            _item_padding.x,
                            _item_padding.y,
                            _on_remove ? 1 : _item_padding.x,
                            _item_padding.y
                        },
                        _item_font
                    }
                }
            }();
            
            chip = ptr.get();
            _flow_objects.insert(_flow_objects.begin() + index, std::move(ptr));
            
            if(_on_remove)
                add_remove();
            
        } else {
            chip = _flow_objects.at(index).to<HorizontalLayout>();
            
            auto front = chip->child<StaticText*>(0);
            front->set(TextClr{_item_text_color});
            front->set(_item_font);
            front->set(SizeLimit{_flow->max_size().width - _item_padding.x * 2 - (_on_remove ? 30 : 0), 0});
            front->set(attr::Margins{
                _item_padding.x,
                _item_padding.y,
                _on_remove ? 1 : _item_padding.x,
                _item_padding.y
            });
            front->set(Str{tag});
            
            if(chip->children().size() == 1
               && _on_remove)
            {
                add_remove();
            } else if(chip->children().size() == 2
                      && not _on_remove)
            {
                chip->remove_child(chip->child<StaticText*>(1));
            } else if(_on_remove) {
                StaticText* remove = chip->child<StaticText*>(1);
                remove->set(TextClr{_item_text_color.alpha(190)});
                remove->set(_item_font);
                remove->set(attr::Margins{
                    0,
                    _item_padding.y,
                    _item_padding.x,
                    _item_padding.y
                });
            }
        }

        chip->set(attr::Margins{0,0, _item_padding.x, _item_padding.y});
        chip->set(FillClr{_item_fill_color.value_or(tag_color(tag))});
        chip->set(LineClr{_item_line_color});
        chip->set(_item_corners);
        chip->set_policy(HorizontalLayout::Policy::CENTER);
    }

    /// remove superfluous items
    assert(_flow_objects.size() >= _tags.size());
    _flow_objects.resize(_tags.size());

    _flow->set_children(_flow_objects);
    _flow->set(Clickable{true});
    refresh_catalog();

    set_content_changed(false);
}

void TagList::refresh_catalog() {
    std::vector<std::string> available;
    available.reserve(_catalog.size());
    for(const auto& candidate : _catalog) {
        if(std::none_of(_tags.begin(), _tags.end(), [&](const auto& tag) {
            return equivalent(tag, candidate);
        })) {
            available.push_back(candidate);
        }
    }
    _dropdown->set_items(std::vector<Dropdown::TextItem>(available.begin(), available.end()));
}

bool TagList::equivalent(std::string_view lhs, std::string_view rhs) {
    return utils::lowercase(trimmed(lhs)) == utils::lowercase(trimmed(rhs));
}

float TagList::similarity(std::string_view lhs, std::string_view rhs) {
    const auto left = utils::lowercase(trimmed(lhs));
    const auto right = utils::lowercase(trimmed(rhs));
    const auto length = max(left.size(), right.size());
    if(length == 0)
        return 1.f;
    return 1.f - float(levenshtein_distance(left, right)) / float(length);
}

void TagList::request_add(std::string value, bool explicit_selection) {
    value = trimmed(value);
    if(value.empty()) {
        _dropdown->clear_textfield();
        _dropdown->set_opened(false);
        return;
    }

    if(not explicit_selection) {
        float best_score = -1.f;
        std::string best;
        auto consider = [&](const std::string& candidate) {
            const auto score = similarity(value, candidate);
            if(score > best_score) {
                best_score = score;
                best = candidate;
            }
        };
        for(const auto& candidate : _catalog)
            consider(candidate);
        for(const auto& candidate : _tags)
            consider(candidate);

        if(best_score >= _match_threshold)
            value = std::move(best);
        else if(not _allow_new)
            return;
    }

    const bool duplicate = std::any_of(_tags.begin(), _tags.end(), [&](const auto& tag) {
        return equivalent(tag, value);
    });
    if(not duplicate && _on_add)
        _on_add(value);

    _dropdown->clear_textfield();
    _dropdown->set_opened(false);
}

void TagList::commit_typed_text() {
    request_add(_dropdown->text(), false);
}

void TagList::position_input() {
    if(not _on_add)
        return;

    const auto input_pos = Vec2();
    _dropdown->set_pos(input_pos);
    _dropdown->set_size(_input_size);

    /*const Bounds input_bounds{input_pos, _input_size};
    const Bounds viewport{{}, _flow->size()};
    const bool visible = input_bounds.y < viewport.height
                      && input_bounds.y + input_bounds.height > 0
                      && input_bounds.x < viewport.width
                      && input_bounds.x + input_bounds.width > 0;
    if(not visible)
        _dropdown->set_opened(false);*/
}

void TagList::update() {
    rebuild_structure();
    
    _flow->set(Loc{0, _on_add ? _dropdown->height() + 10 : 0});

    _flow->update();
    Entangled::set_size(_flow->size() + Size2(0, _on_add ? (10 + _dropdown->height()) : 0));
    position_input();

    auto context = OpenContext();
    advance_wrap(*_flow);
    if(_on_add)
        advance_wrap(*_dropdown);
    
    if(not _on_add) {
        _dropdown->set_opened(false);
    }
}

}
