#ifndef CSVREADER_H
#define CSVREADER_H

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <algorithm>

namespace cmn {

/* ==== Table ================================================================= */

template<typename CellT = std::string>
struct Table {

    /* ---- lightweight row views ---------------------------------------- */

    struct ConstRow;

    struct Row {
        const std::vector<std::string>* header;
        std::vector<CellT>*             cells;

        std::size_t size() const { return cells->size(); }

        CellT& operator[](std::size_t i) { return (*cells)[i]; }
        const CellT& operator[](std::size_t i) const { return (*cells)[i]; }

        std::optional<CellT> get(const std::string& column) const {
            auto it = std::find(header->begin(), header->end(), column);
            if (it == header->end())
                return std::nullopt;
            std::size_t idx = std::distance(header->begin(), it);
            if (idx >= cells->size())
                return std::nullopt;
            return (*cells)[idx];
        }
    };

    struct ConstRow {
        const std::vector<std::string>* header;
        const std::vector<CellT>*       cells;

        std::size_t size() const { return cells->size(); }

        const CellT& operator[](std::size_t i) const { return (*cells)[i]; }

        std::optional<CellT> get(const std::string& column) const {
            auto it = std::find(header->begin(), header->end(), column);
            if (it == header->end())
                return std::nullopt;
            std::size_t idx = std::distance(header->begin(), it);
            if (idx >= cells->size())
                return std::nullopt;
            return (*cells)[idx];
        }
    };

    /* ---- actual table data -------------------------------------------- */

    std::vector<std::string>           header;
    std::vector<std::vector<CellT>>    rows;

    std::size_t size() const { return rows.size(); }

    /* Row accessors */
    Row operator[](std::size_t i) {
        return Row{ &header, &rows[i] };
    }
    ConstRow operator[](std::size_t i) const {
        return ConstRow{ &header, &rows[i] };
    }

    /* Column-based access when the row index is known */
    std::optional<CellT> get(std::size_t row,
                             const std::string& column) const
    {
        if (row >= rows.size())
            return std::nullopt;
        return (*this)[row].get(column);
    }
};

} // namespace cmn

#endif // CSVREADER_H
