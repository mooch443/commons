#include "Dropdown.h"

namespace gui {
    std::atomic_long _automatic_id = 0;
    Dropdown::Item::Item(long ID) : _ID(ID) {
        if(_ID == Item::INVALID_ID) {
            _ID = automatic_id();
        } else
            ++_automatic_id;
    }
    
    long Dropdown::Item::automatic_id() {
        return _automatic_id++;
    }
    
    Dropdown::Dropdown(const Bounds& bounds, const std::vector<std::string>& options, Type type) : Dropdown(bounds, std::vector<TextItem>(options.begin(), options.end()), type)
    { }

using TextItem = Dropdown::TextItem;

class TrieNode {
public:
    TrieNode() : is_end_of_word(false), index(-1) {
        std::fill(children.begin(), children.end(), nullptr);
    }

    void insert(const std::string& key, int idx) {
        TrieNode* node = this;
        for (char c : key) {
            c = std::tolower(c);
            int index = c;
            assert(index < 256 && index >= 0);
            if (!node->children[index]) {
                node->children[index] = std::make_unique<TrieNode>();
            }
            node = node->children[index].get();
        }
        node->is_end_of_word = true;
        node->index = idx;
    }

    std::vector<int> search_by_prefix(const std::string& prefix) const {
        const TrieNode* node = this;
        for (char c : prefix) {
            c = std::tolower(c);
            int index = c;
            if (!node->children[index]) {
                return {};
            }
            node = node->children[index].get();
        }
        std::vector<int> indices;
        collect_indices(node, indices);
        return indices;
    }

private:
    std::array<std::unique_ptr<TrieNode>, 256> children;
    bool is_end_of_word;
    int index;

    void collect_indices(const TrieNode* node, std::vector<int>& indices) const {
        if (node->is_end_of_word) {
            indices.push_back(node->index);
        }
        for (const auto& child : node->children) {
            if (child) {
                collect_indices(child.get(), indices);
            }
        }
    }
};

class Trie {
public:
    Trie() : root(std::make_unique<TrieNode>()) {}

    void insert(const std::string& key, int idx) {
        root->insert(key, idx);
    }

    std::vector<int> search_by_prefix(const std::string& prefix) const {
        return root->search_by_prefix(prefix);
    }

private:
    std::unique_ptr<TrieNode> root;
};



int levenshtein_distance(const std::string &s1, const std::string &s2) {
    size_t len1 = s1.size();
    size_t len2 = s2.size();
    
    std::vector<std::vector<int>> dp(len1 + 1, std::vector<int>(len2 + 1));

    for (size_t i = 0; i <= len1; i++) {
        dp[i][0] = i;
    }
    for (size_t j = 0; j <= len2; j++) {
        dp[0][j] = j;
    }

    for (size_t i = 1; i <= len1; i++) {
        for (size_t j = 1; j <= len2; j++) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            dp[i][j] = std::min({dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost});
        }
    }

    return dp[len1][len2];
}

std::vector<TextItem> filter_items(const Trie& trie, const std::string& search_text, const std::vector<TextItem>& items) {
    // Convert the search text to lowercase
    std::string search_text_lower = search_text;
    std::transform(search_text_lower.begin(), search_text_lower.end(), search_text_lower.begin(), ::tolower);

    // Search for items with matching prefixes
    std::vector<int> matching_indices = trie.search_by_prefix(search_text_lower);

    // Create a priority queue to store the items and their Levenshtein distances
    auto comp = [](const std::pair<int, TextItem>& a, const std::pair<int, TextItem>& b) { return a.first > b.first; };
    std::priority_queue<std::pair<int, TextItem>, std::vector<std::pair<int, TextItem>>, decltype(comp)> pq(comp);

    // Calculate Levenshtein distance for each matching item and store it in the priority queue
    for (int index : matching_indices) {
        const TextItem& item = items[index];
        std::string item_name_lower = item.search_name();
        std::transform(item_name_lower.begin(), item_name_lower.end(), item_name_lower.begin(), ::tolower);

        int distance = levenshtein_distance(search_text_lower, item_name_lower);
        pq.push(std::make_pair(distance, item));
    }

    // Retrieve the items from the priority queue in the order of their relevance
    std::vector<TextItem> filtered_items;
    while (!pq.empty()) {
        filtered_items.push_back(pq.top().second);
        pq.pop();
    }

    return filtered_items;
}

    Dropdown::Dropdown(const Bounds& bounds, const std::vector<TextItem>& options, Type type)
    : _list(Bounds(0, bounds.height, bounds.width, 230), options, Font(0.6f, Align::Left), [this](size_t s) { _selected_item = s; }),
          _on_select([](auto, auto&){}),
          _items(options),
          _opened(false),
          _type(type),
          _inverted(false),
          _selected_item(-1),
          _selected_id(-1),
          _on_text_changed([](){}),
          _on_open([](auto){})
    {
        _list.set_z_index(1);
        
        if(type == BUTTON) {
            _button = Button::MakePtr("Please select...",  Bounds(Vec2(), bounds.size()));
            _button->set_toggleable(true);
            _button->add_event_handler(MBUTTON, [this](Event e){
                if(!e.mbutton.pressed && e.mbutton.button == 0) {
                    _opened = !_opened;
                    this->set_content_changed(true);
                    _on_open(_opened);
                }
            });
            
        } else {
            _textfield = std::make_shared<Textfield>(Bounds(Vec2(), bounds.size()));
            _textfield->set_placeholder("Please select...");
            _textfield->add_event_handler(KEY, [this](Event e){
                if(!e.key.pressed)
                    return;
                
                if(e.key.code == Codes::Down)
                    _selected_item++;
                else if(e.key.code == Codes::Up)
                    _selected_item--;
                
                select_item(_selected_item);
            });
            
            _on_text_changed = [this](){
                if (_custom_on_text_changed) {
                    _custom_on_text_changed(_textfield->text());
                }

                filtered_items.clear();

                if (!_textfield->text().empty()) {
                    // Filter items using the search text
                    std::vector<std::string> corpus;
                    for (const auto &item : _items) {
                        corpus.push_back(item.search_name());
                    }

                    // Get the search result indexes
                    auto search_result_indexes = text_search(_textfield->text(), corpus);

                    // Create a filtered vector of TextItems
                    std::vector<TextItem> filtered;
                    for (int index : search_result_indexes) {
                        filtered.push_back(_items[index]);
                        filtered_items[filtered.size() - 1] = index;
                    }

                    // Update the list with the filtered items
                    _list.set_items(filtered);

                } else {
                    // If the search text is empty, display all items
                    _list.set_items(_items);
                }

                this->select_item(_selected_item);
                this->set_content_changed(true);
            };
            
            _textfield->on_text_changed(_on_text_changed);
            
            _textfield->on_enter([this](){
                if(!_list.items().empty()) {
                    if (stage())
                        stage()->select(NULL);
                    _list.select_highlighted_item();
                } else {
                    if(stage())
                        stage()->select(NULL);
                    _selected_id = -1;
                    _on_select(-1, TextItem());
                }
                
                if(stage())
                    stage()->do_hover(NULL);
                
                _list.set_last_hovered_item(-1);
            });
        }
        
        _list.on_select([this](size_t i, const Dropdown::TextItem& txt){
            long_t real_id = long_t(i);
            if(!filtered_items.empty()) {
                if(filtered_items.find(i) != filtered_items.end()) {
                    real_id = long_t(filtered_items[i]);
                } else
                    throw U_EXCEPTION("Unknown item id ",i," (",filtered_items.size()," items)");
            }
            
            if(_button) {
                _button->set_toggle(_opened);
                _button->set_txt(txt);
                
            } else if(_textfield) {
                _textfield->set_text(txt);
            }
            
            _selected_id = real_id;
            
            /*if(stage()) {
                stage()->select(NULL);
                stage()->do_hover(NULL);
            }*/
            
            _list.set_last_hovered_item(-1);
            
            if(this->selected() != _opened) {
                _opened = this->selected();
                _on_open(_opened);
            }
            
            this->set_content_changed(true);
            
            _on_select(real_id, txt);
        });
        
        set_bounds(bounds);
        set_clickable(true);
        
        if(type == SEARCH)
            add_event_handler(SELECT, [this](Event e) {
                if(e.select.selected)
                    this->set_opened(true);
                else
                    this->set_opened(false);
                this->_on_open(this->_opened);
            });
    }
    
    Dropdown::~Dropdown() {
        _button = nullptr;
        _textfield = nullptr;
    }
    
    void Dropdown::set_opened(bool opened) {
        if(_opened == opened)
            return;
        
        _opened = opened;
        set_content_changed(true);
    }
    
    void Dropdown::select_textfield() {
        if(stage() && _textfield) {
            stage()->select(_textfield.get());
        }
    }
    void Dropdown::clear_textfield() {
        if(_textfield) {
            _textfield->set_text("");
            _on_text_changed();
        }
    }
    
    void Dropdown::update() {
        if(!content_changed())
            return;
        
        begin();
        if(_opened)
            advance_wrap(_list);
        if(_button)
            advance_wrap(*_button);
        else
            advance_wrap(*_textfield);
        end();
    }
    
    void Dropdown::select_item(long index) {
        _selected_item = index;
        
        if(index < 0)
            index = 0;
        else if((size_t)index+1 > _list.items().size())
            index = (long)_list.items().size()-1;
        
        if(_list.items().empty())
            index = -1;
        
        if(index > -1)
            _list.highlight_item(index);
        
        if(index != _selected_item) {
            _selected_item = index;
            set_dirty();
        }
    }
    
    void Dropdown::set_items(const std::vector<TextItem> &options) {
        if(options != _items) {
            _list.set_items(options);
            _items = options;
            _selected_id = _selected_item = -1;
            _on_text_changed();
            set_dirty();
        }
    }
    
    const std::string& Dropdown::text() const {
        if(!_textfield)
            throw U_EXCEPTION("No textfield.");
        return _textfield->text();
    }
    
    Dropdown::TextItem Dropdown::selected_item() const {
        if(selected_id() != -1)
            return (size_t)_selected_item < _list.items().size() ?_list.items().at(_selected_item).value() : TextItem();
        
        return TextItem();
    }
    
    Dropdown::TextItem Dropdown::hovered_item() const {
        if(_list.last_hovered_item() != -1) {
            return (size_t)_list.last_hovered_item() < _list.items().size() ? _list.items().at(_list.last_hovered_item()).value() : TextItem();
        }
        return TextItem();
    }
    
    void Dropdown::update_bounds() {
        if(!bounds_changed())
            return;
        
        Entangled::update_bounds();
        
        if(stage()) {
            auto &gb = global_bounds();
            if(gb.y >= stage()->height()/stage()->scale().y * 0.5) {
                set_inverted(true);
            } else
                set_inverted(false);
        }
        
        if(_textfield)
            _textfield->set_size(Size2(width(), height()).div(scale()));
        _list.set_size(Size2(width() / scale().x, _list.height()));
    }
    
    void Dropdown::set_inverted(bool invert) {
        if(_inverted == invert)
            return;
        
        _inverted = invert;
        if(_inverted)
            _list.set_pos(-Vec2(0, _list.height()));
        else
            _list.set_pos(Vec2(0, height()));
        
        set_content_changed(true);
    }
}
