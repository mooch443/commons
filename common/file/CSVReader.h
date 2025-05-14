#pragma once
#include <commons.pc.h>
#include <unordered_map>
#include <optional>
#include <algorithm>
#include <fstream>
#include <file/Path.h>        // assumes project provides this

namespace cmn {

/**
 * @brief Generic container holding tabular data.
 *
 *  * `CellT` is the (uniform) type stored in each cell – e.g. `std::string`,
 *    `float`, `double`, … .
 *  * The header always remains `std::string`, so datasets keep their column
 *    names independent of the stored cell type.
 */
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

        CellT get(std::string_view column) const {
            auto it = std::find(header->begin(), header->end(), std::string(column));
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

        CellT get(std::string_view column) const {
            auto it = std::find(header->begin(), header->end(), std::string(column));
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

using StringTable = Table<std::string>;

/* ==== Inline numeric‑conversion helpers ================================= */

namespace detail {

template<typename T>
inline std::optional<T> parseNumeric(const std::string& s) {
    try {
        if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
            size_t idx = 0;
            double v = std::stod(s, &idx);
            if (idx != s.size())
                return std::nullopt;
            return static_cast<T>(v);
        } else if constexpr (std::is_integral_v<T>) {
            size_t idx = 0;
            long long v = std::stoll(s, &idx);
            if (idx != s.size())
                return std::nullopt;
            return static_cast<T>(v);
        } else {
            return std::nullopt;           // unsupported type
        }
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace detail

/**
 * @brief Convert a StringTable to a numeric Table with optional cells.
 *
 * Each string cell is parsed to `NumberT`.  Missing or invalid cells become
 * `std::nullopt`.  The resulting table has exactly the same dimensions as the
 * header × rows of the source.
 */
template<typename NumberT>
inline Table<std::optional<NumberT>>
convertToOptionalNumeric(const StringTable& src) {
    Table<std::optional<NumberT>> dst;
    dst.header = src.header;
    dst.rows.reserve(src.rows.size());
    for (const auto& r : src.rows) {
        std::vector<std::optional<NumberT>> nr;
        nr.reserve(dst.header.size());
        for (std::size_t i = 0; i < dst.header.size(); ++i) {
            if (i < r.size())
                nr.emplace_back(detail::parseNumeric<NumberT>(r[i]));
            else
                nr.emplace_back(std::nullopt);               // missing field
        }
        dst.rows.emplace_back(std::move(nr));
    }
    return dst;
}

/**
 * @brief In‑memory CSV parser.
 *
 * The class parses CSV data that is already resident in memory (for example a
 * std::string created inside a test).  The delimiter is a single character that
 * defaults to <code>,</code>.  A minimal but production‑worthy CSV grammar is
 * implemented:
 *
 * * Fields may be *quoted* with <code>"</code>.
 * * Inside a quoted field the delimiter and new‑line characters lose their
 *   special meaning.
 * * A doubled quote <code>""</code> becomes a literal quote in the output
 *   field.
 * * Line endings <code>\n</code>, <code>\r\n</code> and <code>\r</code> are
 *   recognised transparently.
 *
 * Example – reading from a buffer in a unit test:
 * ```
 * const std::string csv = "name;age\n\"Doe, John\";42\n";
 * cmn::CSVReader rdr(csv, ';');
 * ASSERT_TRUE(rdr.hasNext());
 * auto row = rdr.nextRow();          // ["name", "age"]
 * ```
 */
class CSVReader {
public:
    CSVReader(std::string_view data,
              char delimiter = ',',
              bool hasHeader = false);

    /** Reset the reader to new data (pos is rewound to zero). */
    void reset(std::string_view data);

    /** Are there still unread rows? */
    [[nodiscard]] bool hasNext() const;

    /** Return the next row or an empty vector when done. */
    std::vector<std::string> nextRow();

    /** Return the parsed header row (empty if none / not yet parsed). */
    const std::vector<std::string>& header() const { return header_; }

    /** Load entire dataset into a map keyed by the value found in
        the column @p keyColumn (must match a header name). */
    std::unordered_map<std::string,
                       std::unordered_map<std::string, std::string>>
    readAsMap(const std::string& keyColumn = "");

    /** Read all remaining rows and return a Table (header + rows). */
    StringTable readTable();

    /** Change delimiter (use with care between rows). */
    void setDelimiter(char d) { delim_ = d; }

    /** Utility that splits a single CSV line (no trailing newline). */
    static std::vector<std::string> splitLine(std::string_view line,
                                              char delimiter);

    template<typename NumberT>
    Table<std::optional<NumberT>> readNumericTableOptional() {
        return convertToOptionalNumeric<NumberT>(readTable());
    }

private:
    std::string_view data_;
    std::size_t pos_{0};
    char delim_;
    bool hasHeader_{false};
    std::vector<std::string> header_;
};

/**
 * @brief Adapter that consumes a std::istream and uses @ref CSVReader
 *        under the hood.
 *
 * This keeps the I/O layer (files, sockets, …) separated from the actual CSV
 * parsing logic so we can mock the parser easily in unit tests.
 */
class CSVStreamReader {
public:
    CSVStreamReader(const file::Path& path,
                    char delimiter = ';',
                    bool hasHeader = false);

    /** Read a row; returns empty vector at EOF */
    std::vector<std::string> nextRow();

    /** Read the entire stream and return rows as vector of maps. */
    std::unordered_map<std::string,
                       std::unordered_map<std::string, std::string>>
    readAsMap(const std::string& keyColumn = "");

    StringTable readTable();

    const std::vector<std::string>& header() const { return header_; }

    /** Convenience: true if the underlying stream hasn’t reached EOF. */
    [[nodiscard]] bool hasNext() {
        return static_cast<bool>(in_.peek()) && !in_.eof();
    }

    template<typename NumberT>
    Table<std::optional<NumberT>> readNumericTableOptional() {
        return convertToOptionalNumeric<NumberT>(readTable());
    }

private:
    std::ifstream in_;
    char delim_;
    bool hasHeader_{false};
    std::vector<std::string> header_;
};

} // namespace cmn
