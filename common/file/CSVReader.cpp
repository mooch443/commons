#include "CSVReader.h"

namespace {

struct posix_closer {
    void operator()(int* fd) const { if (*fd != -1) ::close(*fd); }
};

} // anonymous

namespace cmn {

/* ==== FileBuffer ======================================================== */

FileBuffer::FileBuffer(const file::Path& path) {
#ifdef _WIN32
    hFile_ = ::CreateFileA(path.string().c_str(), GENERIC_READ,
                           FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile_ == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot open file: " + path.string());

    LARGE_INTEGER li{};
    if (!::GetFileSizeEx(hFile_, &li))
        throw std::runtime_error("GetFileSizeEx failed");

    size_ = static_cast<std::size_t>(li.QuadPart);
    hMap_ = ::CreateFileMappingA(hFile_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap_)
        throw std::runtime_error("CreateFileMapping failed");

    data_ = static_cast<const char*>(::MapViewOfFile(hMap_, FILE_MAP_READ, 0, 0, 0));
    if (!data_)
        throw std::runtime_error("MapViewOfFile failed");
#else
    fd_ = ::open(path.c_str(), O_RDONLY);
    if (fd_ == -1)
        throw std::runtime_error("Cannot open file: " + path.str());

    ::off_t len = ::lseek(fd_, 0, SEEK_END);
    size_ = static_cast<std::size_t>(len);
    data_ = static_cast<const char*>(::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0));
    if (data_ == MAP_FAILED) {
        ::close(fd_);
        throw std::runtime_error("mmap failed");
    }
#endif
}

FileBuffer::~FileBuffer() {
#ifdef _WIN32
    if (data_) ::UnmapViewOfFile(data_);
    if (hMap_) ::CloseHandle(hMap_);
    if (hFile_ != INVALID_HANDLE_VALUE) ::CloseHandle(hFile_);
#else
    if (data_) ::munmap(const_cast<char*>(data_), size_);
    if (fd_ != -1) ::close(fd_);
#endif
}

/* ==== CSVReader (memory) ================================================ */

CSVReader::CSVReader(std::string_view data,
                     char delimiter,
                     bool hasHeader)
    : data_(data),
      pos_(0),
      delim_(delimiter),
      hasHeader_(hasHeader)
{
    if (hasHeader_) {
        header_ = nextRow();   // consume header row
    }
}

void CSVReader::reset(std::string_view data) {
    data_ = data;
    pos_  = 0;
    header_.clear();
    if (hasHeader_) {
        header_ = nextRow();
    }
}

bool CSVReader::hasNext() const {
    return pos_ < data_.size();
}

std::vector<std::string> CSVReader::nextRow() {
    if (!hasNext())
        return {};

    std::vector<std::string> row;
    std::string              field;
    bool                     inQuotes = false;

    const std::size_t n = data_.size();
    for (; pos_ < n; ++pos_) {
        char ch = data_[pos_];

        // Handle quoting ("" => literal ")
        if (ch == '"') {
            if (inQuotes && pos_ + 1 < n && data_[pos_ + 1] == '"') {
                field.push_back('"');
                ++pos_;
            } else {
                inQuotes = !inQuotes;
            }
            continue;
        }

        // Delimiter outside quotes
        if (ch == delim_ && !inQuotes) {
            row.emplace_back(std::move(field));
            field.clear();
            continue;
        }

        // Newline outside quotes ends the row
        if ((ch == '\n' || ch == '\r') && !inQuotes) {
            row.emplace_back(std::move(field));
            field.clear();

            // swallow LF in CRLF
            if (ch == '\r' && pos_ + 1 < n && data_[pos_ + 1] == '\n')
                ++pos_;
            ++pos_;
            break;
        }

        // Regular character
        field.push_back(ch);
    }

    // Handle EOF final field
    if (pos_ >= n && !field.empty()) {
        row.emplace_back(std::move(field));
    }

    return row;
}

std::unordered_map<std::string,
                   std::unordered_map<std::string, std::string>>
CSVReader::readAsMap(const std::string& keyColumn)
{
    std::unordered_map<std::string,
                       std::unordered_map<std::string, std::string>> result;

    if (header_.empty())
        return result;

    std::size_t keyIdx;
    if (keyColumn.empty()) {
        keyIdx = 0;
    } else {
        auto it = std::find(header_.begin(), header_.end(), keyColumn);
        if (it == header_.end())
            return result;                // key column not found
        keyIdx = std::distance(header_.begin(), it);
    }

    while (hasNext()) {
        auto row = nextRow();
        row.resize(header_.size());       // pad / truncate

        const std::string& key = row[keyIdx];

        std::unordered_map<std::string, std::string> m;
        for (std::size_t i = 0; i < header_.size(); ++i) {
            m.emplace(header_[i], row[i]);
        }
        result.emplace(key, std::move(m));
    }

    return result;
}

std::size_t CSVReader::fastLineCount() const {
    const char* p   = data_.data();
    const char* end = p + data_.size();
    // use std::count because most libcs use vectorized memchr internally
    std::size_t n = static_cast<std::size_t>(
          std::count(p, end, '\n'));
    if (data_.size() && *(end - 1) != '\n')
        ++n;                    // last line without trailing newline
    return n;
}

StringTable CSVReader::readTable() {
    StringTable t;
    t.header = header_;
    t.rows.reserve(fastLineCount() - (header_.empty() ? 0 : 1));

    while (hasNext()) {
        auto row = nextRow();
        row.resize(t.header.size());
        t.rows.emplace_back(std::move(row));
    }
    return t;
}

std::vector<std::string>
CSVReader::splitLine(std::string_view line, char delimiter)
{
    CSVReader tmp(line, delimiter, false);
    return tmp.nextRow();
}

/* ==== CSVStreamReader (mmap adapter) ==================================== */

CSVStreamReader::CSVStreamReader(const file::Path& path,
                                 char delimiter,
                                 bool hasHeader)
    : buf_(path),
      rdr_(buf_.view(), delimiter, hasHeader)
{}

std::unordered_map<std::string,
                   std::unordered_map<std::string, std::string>>
CSVStreamReader::readAsMap(const std::string& key) {
    return rdr_.readAsMap(key);
}

StringTable CSVStreamReader::readTable() {
    return rdr_.readTable();
}

template<typename NumberT>
Table<std::optional<NumberT>>
CSVStreamReader::readNumericTableOptional() {
    return rdr_.readNumericTableOptional<NumberT>();
}

// Explicit template instantiations
template Table<std::optional<double>> CSVStreamReader::readNumericTableOptional<double>();
template Table<std::optional<float>>  CSVStreamReader::readNumericTableOptional<float>();
template Table<std::optional<int>>    CSVStreamReader::readNumericTableOptional<int>();

} // namespace cmn
