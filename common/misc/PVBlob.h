#pragma once

#include <commons.pc.h>
#include <misc/vec2.h>
#include <misc/Blob.h>
#include <processing/Background.h>
#include <misc/Image.h>
#include <misc/Grid.h>
#include <misc/ProximityGrid.h>
#include <misc/vec2.h>
#include <misc/bid.h>
#include <file/DataFormat.h>

namespace Output {
    class ResultsFormat;
}

namespace pv {

struct Moments {
    float m[3][3];
    float mu[3][3];
    float mu_[3][3];
    
    bool ready;
    
    Moments() {
        ready = false;
        
        for(int i=0; i<3; i++) {
            for(int j=0; j<3; j++) {
                m[i][j] = mu[i][j] = mu_[i][j] = 0;
            }
        }
    }
};

class Blob {
    struct {
        float angle{0};
        cmn::Vec2 center;
        uint64_t _num_pixels{0};
        bool ready{false};
        
    } _properties;
    
    cmn::Bounds _bounds;
    
    std::unique_ptr<std::vector<cmn::HorizontalLine>> _hor_lines;
    GETTER(Moments, moments);
    
public:
    static auto Make(auto && ...args) {
        return std::make_unique<Blob>(std::forward<decltype(args)>(args)...);
    }
    
    const decltype(_properties)& properties() const { return _properties; }
    static size_t all_blobs();
    
    //void add_offset(const cmn::Vec2& off);
    
    inline const std::unique_ptr<std::vector<cmn::HorizontalLine>>& lines() const {
        return _hor_lines;
    }
    inline std::unique_ptr<std::vector<cmn::HorizontalLine>>&& steal_lines() {
        return std::move(_hor_lines);
    }
    inline const std::vector<cmn::HorizontalLine>& hor_lines() const {
        return *_hor_lines;
    }
    
    float orientation() const {
#ifndef NDEBUG
        if(!_moments.ready)
            throw cmn::U_EXCEPTION("Moments aren't ready yet.");
#endif
        return _properties.angle;
    }
    const decltype(_properties.center)& center() const {
#ifndef NDEBUG
        if(!_properties.ready)
            throw cmn::U_EXCEPTION("Properties aren't ready yet.");
#endif
        return _properties.center;
    }
    const decltype(_bounds)& bounds() const {
#ifndef NDEBUG
        if(!_properties.ready)
            throw cmn::U_EXCEPTION("Properties have not been calculated yet.");
#endif
        return _bounds;
    }
    uint64_t num_pixels() const {
#ifndef NDEBUG
        if(!_properties.ready)
            throw cmn::U_EXCEPTION("Properties have not been calculated yet.");
#endif
        return _properties._num_pixels;
    }
    void sort() {
        std::sort(_hor_lines->begin(), _hor_lines->end(), std::less<cmn::HorizontalLine>());
    }
    
    std::unique_ptr<std::vector<uchar>> calculate_pixels(const cv::Mat& background) const {
        auto res = std::make_unique<std::vector<uchar>>();
        res->resize(num_pixels());
        auto ptr = res->data();
        
        for(auto &hl : *_hor_lines) {
            for (ushort x=hl.x0; x<=hl.x1; ++x, ++ptr) {
                assert(ptr < res->data() + res->size());
                *ptr = background.at<uchar>(hl.y, x);
            }
        }
        return res;
    }
    
    bool properties_ready() const { return _properties.ready; }
    
    std::string toStr() const;
    static std::string class_name() {
        return "Blob";
    }
    
    //UTILS_TOSTRING("Blob<pos:" << (cv::Point2f)center() << " size:" << (cv::Size)_bounds.size() << ">");
    
public:
    void calculate_properties();
    void calculate_moments();
    
public:
    enum class Flags {
        split = 1,
        is_tag = 2,
        is_instance_segmentation = 4
    };
    
    static void set_flag(uint8_t &flags, Flags flag, bool v) {
        flags ^= (-uint8_t(v) ^ flags) & (1UL << uint8_t(flag));
    }

    static bool is_flag(uint8_t flags, Flags flag) {
        return (flags >> uint8_t(flag)) & 1u;
    }
    
    static constexpr uint8_t flag(Flags flag) {
        return uint8_t(1UL << uint8_t(flag));
    }
    
protected:
    GETTER_NCONST(cmn::blob::pixel_ptr_t, pixels)
    GETTER_I(uint8_t, flags, 0)
    GETTER_I(bid, parent_id, pv::bid::invalid)
    GETTER_I(bid, blob_id, pv::bid::invalid)
    GETTER_SETTER(bool, tried_to_split)
    GETTER_SETTER_I(FilterReason, reason, FilterReason::Unknown)
    GETTER_SETTER(cmn::blob::Prediction, prediction)
    
    float _recount;
    int32_t _recount_threshold{-1};
    uchar _color_percentile_5, _color_percentile_95;
    
public:
    Blob();
    Blob(cmn::blob::line_ptr_t&& lines, cmn::blob::pixel_ptr_t&& pixels, uint8_t flags, cmn::blob::Prediction&& pred);
    //Blob(blob::line_ptr_t&& lines, blob::pixel_ptr_t&& pixels);
    Blob(const cmn::blob::line_ptr_t::element_type& lines, const cmn::blob::pixel_ptr_t::element_type& pixels, uint8_t flags, cmn::blob::Prediction pred);
    Blob(const cmn::blob::line_ptr_t::element_type& lines, uint8_t flags);
    //Blob(const cmn::Blob* blob, cmn::blob::pixel_ptr_t&& pixels);
    Blob(const pv::Blob& other);
    Blob(Blob&&) = default;
    
    Blob& operator=(Blob&&) = default;
    Blob& operator=(const Blob&) = delete;
    
    ~Blob();
    
    bool split() const;
    
    bool is_tag() const;
    bool is_instance_segmentation() const;
    void set_tag(bool);
    void set_instance_segmentation(bool);
    
    BlobPtr threshold(int32_t value, const cmn::Background& background);
    std::tuple<cmn::Vec2, std::unique_ptr<cmn::Image>> image(const cmn::Background* background = NULL, const cmn::Bounds& restricted = cmn::Bounds(-1,-1,-1,-1), uchar padding = 1) const;
    std::tuple<cmn::Vec2, std::unique_ptr<cmn::Image>> alpha_image(const cmn::Background& background, int32_t threshold) const;
    std::tuple<cmn::Vec2, std::unique_ptr<cmn::Image>> difference_image(const cmn::Background& background, int32_t threshold) const;
    std::tuple<cmn::Vec2, std::unique_ptr<cmn::Image>> thresholded_image(const cmn::Background& background, int32_t threshold) const;

    [[deprecated("Please use the non-moving version instead.")]] std::tuple<cmn::Vec2, std::unique_ptr<cmn::Image>> luminance_alpha_image(const cmn::Background& background, int32_t threshold, uint8_t padding = 1) const;
    cmn::Vec2 luminance_alpha_image(const cmn::Background& background, int32_t threshold, cmn::Image& image, uint8_t padding = 1) const;

    [[deprecated("Please use the non-moving version instead.")]] std::tuple<cmn::Vec2, std::unique_ptr<cmn::Image>> equalized_luminance_alpha_image(const cmn::Background& background, int32_t threshold, float minimum, float maximum, uint8_t padding = 1) const;
    cmn::Vec2 equalized_luminance_alpha_image(const cmn::Background& background, int32_t threshold, float minimum, float maximum, cmn::Image& image, uint8_t padding = 1) const;
    std::tuple<cmn::Vec2, std::unique_ptr<cmn::Image>> binary_image(const cmn::Background& background, int32_t threshold) const;
    std::tuple<cmn::Vec2, std::unique_ptr<cmn::Image>> binary_image() const;
    
    void set_pixels(cmn::blob::pixel_ptr_t&& pixels);
    //void set_pixels(const cmn::grid::PixelGrid &grid, const cmn::Vec2& offset = cmn::Vec2(0));
    
    static decltype(_pixels) calculate_pixels(const cmn::Image::Ptr& image, const decltype(_hor_lines)& lines, const cmn::Vec2& offset = cmn::Vec2(0,0));
    
    int32_t last_recount_threshold() const;
    float raw_recount(int32_t threshold) const;
    float raw_recount(int32_t threshold, const cmn::Background&, bool dont_cache = false);
    float recount(int32_t threshold) const;
    float recount(int32_t threshold, const cmn::Background&, bool dont_cache = false);
    
    void force_set_recount(int32_t threshold, float value = -1);
    void transfer_backgrounds(const cmn::Background& from, const cmn::Background& to, const cmn::Vec2& dest_offset = cmn::Vec2());
    
    void set_split(bool split, const pv::BlobPtr& parent);
    
    std::string name() const;
    void add_offset(const cmn::Vec2& off);
    void scale_coordinates(const cmn::Vec2& scale);
    size_t memory_size() const;
    
    bool operator!=(const pv::Blob& other) const;
    bool operator==(const pv::Blob& other) const;
    bool operator==(const pv::bid& bdx) const {
        return blob_id() == bdx;
    }
    
protected:
    friend class ::Output::ResultsFormat;
    friend struct CompressedBlob;
    friend class cmn::DataFormat;
    friend class cmn::DataPackage;
    
    void set_split(bool);
    void set_parent_id(const bid& parent_id);
    void init();
};

//! comparing a blob pointer to a bid should work like this:
inline bool operator==(const pv::BlobPtr& A, const pv::bid& bdx) {
    return A && A->blob_id() == bdx;
}

struct ShortHorizontalLine {
private:
    //! starting and end position on x
    //  the last bit of _x1 is a flag telling the program
    //  whether this line is the last line on the current y coordinate.
    //  the following lines are on current_y + 1.
    uint16_t _x0, _x1;
    
public:
    //! compresses an array of HorizontalLines to an array of ShortHorizontalLines
    static std::vector<ShortHorizontalLine> compress(const std::vector<cmn::HorizontalLine>& lines);
    //! uncompresses an array of ShortHorizontalLines back to HorizontalLines
    static cmn::blob::line_ptr_t uncompress(uint16_t start_y, const std::vector<ShortHorizontalLine>& compressed);
    
public:
    constexpr ShortHorizontalLine() : _x0(0), _x1(0) {}
    
    constexpr ShortHorizontalLine(uint16_t x0, uint16_t x1, bool eol = false)
        : _x0(x0), _x1((x1 & 0x7FFF) | uint16_t(eol << 15))
    {
        assert(x1 < 32768); // MAGIC NUMBERZ (uint16_t - 1 bit)
    }
    
    constexpr uint16_t x0() const { return _x0; }
    constexpr uint16_t x1() const { return _x1 & 0x7FFF; }
    
    //! returns true if this is the last element on the current y coordinate
    //  if true, the following lines are on current_y + 1.
    //  @note stored in the last bit of _x1
    constexpr bool eol() const { return (_x1 & 0x8000) != 0; }
    constexpr void eol(bool v) { _x1 = (_x1 & 0x7FFF) | uint16_t(v << 15); }
};

//! Contains all important data from Blobs for storing on-disk or compressing memory.
//! Less versatile though, and less fast to use.
struct CompressedBlob {
    //! this represents parent_id (2^1), split (2^0) and tried_to_split (2^2)
    uint8_t status_byte = 0;
    pv::bid parent_id = pv::bid::invalid;
    mutable pv::bid own_id = pv::bid::invalid;

    //! y of first position (everything is relative to this)
    uint16_t start_y{0};
    
    cmn::blob::Prediction pred;
    
protected:
    GETTER(std::vector<ShortHorizontalLine>, lines);
    
    friend struct MemoryStats;
    friend class Output::ResultsFormat;

public:
    CompressedBlob() = default;
    CompressedBlob(const pv::Blob& val) :
        parent_id(val.parent_id()),
        own_id(val.blob_id()),
        pred(val.prediction())
    {
        status_byte = (uint8_t(val.split())             << 0)
                    | (uint8_t(val.parent_id().valid()) << 1)
                    | (uint8_t(val.tried_to_split())    << 2)
                    | (uint8_t(val.is_tag())            << 3)
                    | (uint8_t(val.is_instance_segmentation()) << 4);
        _lines = ShortHorizontalLine::compress(val.hor_lines());
        start_y = val.lines()->empty() ? 0 : val.lines()->front().y;
    }
        
    bool split() const { return status_byte & 0x1; }
    bool is_tag() const { return (status_byte >> 3) & 1u; }
    bool is_instance_segmentation() const { return (status_byte >> 4) & 1u; }
    cmn::Bounds calculate_bounds() const;
        
    pv::BlobPtr unpack() const;
    uint64_t num_pixels() const {
        // adding +1 to result for each line (in order to include x1 as part of the total count)
        uint64_t result = _lines.size();
            
        // adding all line lengths
        for(auto &line : _lines)
            result += line.x1() - line.x0();
            
        return result;
    }
        
    constexpr const bid& blob_id() const {
        return own_id;
    }
    
    void reset_id() {
        own_id = pv::bid::from_blob(*this);
    }
    
    std::string toStr() const;
    static std::string class_name() { return "CompressedBlob"; }
};

}
