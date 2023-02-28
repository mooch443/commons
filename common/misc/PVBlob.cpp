#include <misc/PVBlob.h>
#include <misc/GlobalSettings.h>
#include <misc/Timer.h>
#include <misc/create_struct.h>

namespace pv {
    using namespace cmn;
    using namespace cmn::blob;


CREATE_STRUCT(PVSettings,
    (float, cm_per_pixel),
    (bool, correct_illegal_lines)
)

#define setting(NAME) PVSettings::get<PVSettings :: NAME>()

std::string CompressedBlob::toStr() const {
    return "CompressedBlob<"+Meta::toStr(blob_id())+" parent:"+Meta::toStr(parent_id)+" split:"+Meta::toStr(split())+">";
}


//#define _DEBUG_MEMORY
#ifdef _DEBUG_MEMORY
std::unordered_map<Blob*, std::tuple<int, std::shared_ptr<void*>>> all_blobs_map;
std::mutex all_mutex_blob;
#endif

Blob::~Blob() {
#ifdef _DEBUG_MEMORY
    std::lock_guard<std::mutex> guard(all_mutex_blob);
    auto it = all_blobs_map.find(this);
    if(it == all_blobs_map.end())
        FormatError("Double delete?");
    else
        all_blobs_map.erase(it);
#endif
}

size_t Blob::all_blobs() {
#ifdef _DEBUG_MEMORY
    std::lock_guard<std::mutex> guard(all_mutex_blob);
    
    std::set<std::string> resolved, drawbles;
    for(auto && [ptr, tuple] : all_blobs_map) {
        resolved.insert(resolve_stacktrace(tuple));
    }
    auto str = "[Blobs]\n"+Meta::toStr(resolved);
    
    auto f = fopen("blobs.log", "wb");
    if(f) {
        fwrite(str.data(), sizeof(char), str.length(), f);
        fclose(f);
    } else
        FormatError("Cannot write 'blobs.log'");
    return all_blobs_map.size();
#else
    return 0;
#endif
}

void Blob::calculate_moments() {
    if(_moments.ready)
        return;
    
    assert(_properties.ready);
    for (auto &h: *_hor_lines) {
        const uint my = h.y;
        const uint mysq = my * my;
        
        int mx = h.x0;
        
        for (int x=h.x0; x<=h.x1; x++, mx++) {
            const uint mxsq = mx * mx;
            
            _moments.m[0][0] += 1;
            _moments.m[0][1] += 1 * my;
            _moments.m[1][0] += mx * 1;
            _moments.m[1][1] += mx * my;
            
            _moments.m[2][0] += mxsq;
            _moments.m[0][2] += mysq;
            _moments.m[2][1] += mxsq * my;
            _moments.m[1][2] += mx * mysq;
            _moments.m[2][2] += mxsq * mysq;
        }
    }
    
    _properties.center = Vec2(_moments.m[1][0] / _moments.m[0][0],
                              _moments.m[0][1] / _moments.m[0][0]);
    
    for (auto &h: *_hor_lines) {
        const int vy  = (h.y) - _properties.center.y;
        const int vy2 = vy * vy;
        
        int vx = (h.x0) - _properties.center.x;
        
        for (int x=h.x0; x<=h.x1; x++, vx++) {
            const auto vx2 = vx * vx;

            _moments.mu[0][0] += 1;
            _moments.mu[0][1] += 1 * vy;
            _moments.mu[0][2] += 1 * vy2;

            _moments.mu[1][0] += vx * 1;
            _moments.mu[1][1] += float(vx) * float(vy);
            _moments.mu[1][2] += float(vx) * float(vy2);

            _moments.mu[2][0] += vx2 * 1;
            _moments.mu[2][1] += float(vx2) * float(vy);
            _moments.mu[2][2] += float(vx2) * float(vy2);
        }
    }

    float mu00_inv = 1.0f / float(_moments.mu[0][0]);
    for (int i=0; i<3; i++) {
        for (int j=0; j<3; j++) {
            _moments.mu_[i][j] = _moments.mu[i][j] * mu00_inv;
        }
    }
    
    _properties.angle = 0.5 * cmn::atan2(2 * _moments.mu_[1][1], _moments.mu_[2][0] - _moments.mu_[0][2]);
    _moments.ready = true;
}

void Blob::calculate_properties() {
    if(_properties.ready)
        return;
    
    // find x,y and width,height of blob
    int x = INT_MAX, y = INT_MAX, maxx = 0, maxy = 0;
    int num_pixels = 0;
    
    for (auto &h: *_hor_lines) {
        if (h.x0 < x)
            x = h.x0;
        if (h.x1 > maxx)
            maxx = h.x1;
        
        if (h.y > maxy)
            maxy = h.y;
        if (h.y < y)
            y = h.y;
        
        num_pixels += h.x1-h.x0+1;
    }
    
    _bounds = Bounds(x, y, maxx-x+1, maxy-y+1);
    _properties._num_pixels = num_pixels;
    _properties.center = _bounds.pos() + _bounds.size() * 0.5;
    
    _properties.ready = true;
}

bool Blob::operator!=(const pv::Blob& other) const {
    return blob_id() != other.blob_id();
}

bool Blob::operator==(const pv::Blob& other) const {
    return blob_id() == other.blob_id();
}

cmn::Bounds CompressedBlob::calculate_bounds() const {
    Float2_t max_x = 0, height = 0, min_x = _lines.empty() ? 0 : infinity<Float2_t>();
    Float2_t x0, x1;
    for(auto &line : _lines) {
        x0 = line.x0();
        x1 = line.x1();
        if(x1 > max_x) max_x = x1;
        if(x0 < min_x) min_x = x0;
        if(line.eol()) ++height;
    }
    
    return cmn::Bounds(min_x, start_y, max_x - min_x + 1, height + 1);
}

pv::BlobPtr CompressedBlob::unpack() const {
    auto flines = ShortHorizontalLine::uncompress(start_y, _lines);
    auto ptr = pv::Blob::Make(std::move(flines), nullptr, 0, blob::Prediction{pred});
    ptr->set_parent_id((status_byte & 0x2) != 0 ? parent_id : pv::bid::invalid);
    
    bool tried_to_split = (status_byte & 0x4) != 0;
    ptr->set_tried_to_split(tried_to_split);
    
    if((status_byte & 0x1) != 0 && (status_byte & 0x2) == 0) {
        ptr->set_split(true);
    } else
        ptr->set_split(false);
    
    ptr->set_tag(is_tag());
    
    return ptr;
}

    bool Blob::split() const {
        return Blob::is_flag(_flags, Flags::split);
    }

    void Blob::set_split(bool split) {
        Blob::set_flag(_flags, Flags::split, split);
    }

    bool Blob::is_tag() const {
        return Blob::is_flag(_flags, Flags::is_tag);
    }

    void Blob::set_tag(bool v) {
        Blob::set_flag(_flags, Flags::is_tag, v);
    }

    std::vector<ShortHorizontalLine>
        ShortHorizontalLine::compress(const std::vector<HorizontalLine>& lines)
    {
        std::vector<pv::ShortHorizontalLine> ret((NoInitializeAllocator<pv::ShortHorizontalLine>()));
        ret.resize(lines.size());
        
        auto start = lines.data(), end = lines.data() + lines.size();
        auto rptr = ret.data();
        auto prev_y = ret.empty() ? 0 : lines.front().y;
        
        for(auto lptr = start; lptr != end; lptr++, rptr++) {
            *rptr = pv::ShortHorizontalLine(lptr->x0, lptr->x1);
            
            if(prev_y != lptr->y)
                (rptr-1)->eol(true);
            prev_y = lptr->y;
        }
        
        return ret;
    }
    
    blob::line_ptr_t ShortHorizontalLine::uncompress(
        uint16_t start_y,
        const std::vector<ShortHorizontalLine>& compressed)
    {
        auto uncompressed = std::make_unique<std::vector<HorizontalLine>>((NoInitializeAllocator<HorizontalLine>()));
        uncompressed->resize(compressed.size());
        
        auto y = start_y;
        auto uptr = uncompressed->data();
        auto cptr = compressed.data(), end = compressed.data()+compressed.size();
        
        for(; cptr != end; cptr++, uptr++) {
            uptr->y = y;
            uptr->x0 = cptr->x0();
            uptr->x1 = cptr->x1();
            
            if(cptr->eol())
                y++;
        }
        
        return uncompressed;
    }
    
    Blob::Blob()
        : Blob(std::make_unique<std::vector<HorizontalLine>>(), nullptr, 0, {})
    { }

    Blob::Blob(const line_ptr_t::element_type& lines,
               const pixel_ptr_t::element_type& pixels,
               uint8_t flags,
               Prediction pred)
        : Blob(std::make_unique<line_ptr_t::element_type>(lines),
               std::make_unique<pixel_ptr_t::element_type>(pixels),
               flags,
               std::move(pred))
    { }

    Blob::Blob(const line_ptr_t::element_type& lines,
               uint8_t flags)
        : Blob(std::make_unique<line_ptr_t::element_type>(lines), nullptr, flags, {})
    { }
    
    Blob::Blob(const Blob& other)
      : _properties(other._properties),
        _bounds(other._bounds),
        _hor_lines(std::make_unique<line_ptr_t::element_type>(*other.lines())),
        _moments(other._moments),
        _pixels(other.pixels()
                ? std::make_unique<pixel_ptr_t::element_type>(*other.pixels())
                : nullptr),
        _flags(other.flags()),
        _parent_id(other._parent_id),
        _blob_id(other._blob_id),
        _tried_to_split(other._tried_to_split),
        _reason(other._reason),
        _prediction(other._prediction),
        _recount(other._recount),
        _recount_threshold(other._recount_threshold),
        _color_percentile_5(other._color_percentile_5),
        _color_percentile_95(other._color_percentile_95)
    {
        
    }
    
    Blob::Blob(line_ptr_t&& lines, pixel_ptr_t&& pixels, uint8_t flags, Prediction&& pred)
        : _hor_lines(std::move(lines)), _pixels(std::move(pixels)), _prediction(std::move(pred))
    {
    #ifdef _DEBUG_MEMORY
        std::lock_guard<std::mutex> guard(all_mutex_blob);
        all_blobs_map[this] = retrieve_stacktrace();
    #endif
        
        if(!_hor_lines)
            throw U_EXCEPTION("Blob initialized with NULL lines array");
        
        init();
        _flags |= flags;
    }
    /*Blob::Blob(const cmn::Blob* blob, pixel_ptr_t&& pixels)
        : Blob(std::make_unique<line_ptr_t::element_type>(*blob->lines()),
               std::move(pixels),
               0)
    { }*/
    
    void Blob::init() {
        PVSettings::init();
        
        _tried_to_split = false;
        _flags = 0x0;
        
        _recount = _recount_threshold = -1;
        _parent_id = bid::invalid;
        
//#ifndef NDEBUG
        static std::atomic_int counter(0);
        static std::atomic_bool displayed_warning_once(false);
        if(setting(correct_illegal_lines) || (!displayed_warning_once && counter < 1000)) {
            ++counter;
            
            HorizontalLine prev = hor_lines().empty() ? HorizontalLine() : hor_lines().front();
            
            bool incorrect = false;
            for (auto &line : hor_lines()) {
                if(!(prev == line) && !(prev < line)) {
                    if(!displayed_warning_once) {
                        FormatWarning("HorizontalLines are not properly ordered, or overlapping in x [", prev.x0,"-", prev.x1,"] < [", line.x0,"-", line.x1,"] (", prev.y,"/", line.y,"). Please set 'correct_illegal_lines' = true in your settings if you havent already.");
                        displayed_warning_once = true;
                    }
                    incorrect = true;
                    break;
                }
                prev = line;
            }
            
            if(incorrect) {
                if(_pixels) {
                    std::vector<uchar> pixels(_pixels->begin(), _pixels->end());
                    std::vector<HorizontalLine> lines(_hor_lines->begin(), _hor_lines->end());
                    HorizontalLine::repair_lines_array(lines, pixels);
                    _hor_lines = std::make_unique<line_ptr_t::element_type>(lines);
                    _pixels = std::make_unique<pixel_ptr_t::element_type>(pixels);
                } else {
                    std::vector<HorizontalLine> lines(_hor_lines->begin(), _hor_lines->end());
                    HorizontalLine::repair_lines_array(lines);
                    _hor_lines = std::make_unique<line_ptr_t::element_type>(lines);
                }
            }
        }
//#endif
        
        calculate_properties();
        _blob_id = bid::from_blob(*this);
    }

    int32_t Blob::last_recount_threshold() const {
        return _recount_threshold;
    }
    
    void Blob::set_split(bool split, const pv::BlobPtr& parent) {
        set_split(split);
        
        if(parent) {
            _parent_id = parent->parent_id().valid() ? parent->parent_id() : parent->blob_id();
            set_tag(parent->is_tag());
        } else
            _parent_id = bid::invalid;
        
        if(!_parent_id.valid() && split)
            print("Parent has to be set correctly in order to split blobs (",blob_id(),").");
    }
    
    void Blob::set_parent_id(const bid& parent_id) {
        set_split(parent_id.valid());
        _parent_id = parent_id;
    }
    
    void Blob::force_set_recount(int32_t threshold, float value) {
        if(threshold && _recount_threshold == threshold) {
            //print("Not forcing recount of ",blob_id()," because it has already been calculated.");
            return;
        }
        
        _recount = (value != -1 ? value : num_pixels());
        _recount_threshold = threshold;
    }
    
    float Blob::recount(int32_t threshold, const cmn::Background &background, bool dont_cache) {
        if(setting(cm_per_pixel) == 0) {
            throw U_EXCEPTION("cm_per_pixel is set to 0, which will result in all object sizes being 0 as well. Please set it before tracking.");
        }
        return raw_recount(threshold, background, dont_cache) * SQR(setting(cm_per_pixel));
    }

    float Blob::raw_recount(int32_t threshold, const Background& background, bool dont_cache) {
        if(threshold == 0) {
            if(dont_cache && _recount_threshold == 0)
                return _recount;
            
            _recount = num_pixels();
            _recount_threshold = 0;
            return _recount;
        }
        
        if(threshold == -1 && _recount_threshold == -1)
            throw U_EXCEPTION("Did not calculate recount yet.");
        
        if(threshold >= 0 && _recount_threshold != threshold) {
            //if(_recount_threshold != -1)
            
            if(_pixels == nullptr)
                throw U_EXCEPTION("Cannot threshold without pixel values.");
            
            float recount = 0;
#ifndef NDEBUG
            size_t local_recount = 0;
            auto local_ptr = _pixels->data();
#endif
            auto ptr = _pixels->data();
            if(Background::enable_absolute_difference()) {
                for (auto &line : hor_lines()) {
                    recount += background.count_above_threshold<DifferenceMethod::absolute>(line.x0, line.x1, line.y, ptr, threshold);
                    ptr += ptr_safe_t(line.x1) - ptr_safe_t(line.x0) + 1;
#ifndef NDEBUG
                    for (auto x=line.x0; x<=line.x1; ++x, ++local_ptr) {
                        if(background.is_different<DifferenceMethod::absolute>(x, line.y, *local_ptr, threshold)) {
                            local_recount++;
                        }
                    }
#endif
                }
            } else {
                for (auto &line : hor_lines()) {
                    recount += background.count_above_threshold<DifferenceMethod::sign>(line.x0, line.x1, line.y, ptr, threshold);
                    ptr += ptr_safe_t(line.x1) - ptr_safe_t(line.x0) + 1;
#ifndef NDEBUG
                    for (auto x=line.x0; x<=line.x1; ++x, ++local_ptr) {
                        if(background.is_different<DifferenceMethod::sign>(x, line.y, *local_ptr, threshold)) {
                            local_recount++;
                        }
                    }
#endif
                }
            }
            
            assert(recount == local_recount);
            
            if(!dont_cache || _recount_threshold == -1) {
                _recount_threshold = threshold;
                _recount = recount;
            } else
                return recount;
        }
        
        return _recount;
    }
    
    float Blob::raw_recount(int32_t threshold) const {
        if(threshold != -1 && _recount_threshold != threshold)
            throw U_EXCEPTION("Have to threshold() first.");
        if(threshold == -1 && _recount_threshold == -1)
            throw U_EXCEPTION("Did not calculate recount yet.");
        
        return _recount;
    }

    float Blob::recount(int32_t threshold) const {
        return raw_recount(threshold) * SQR(setting(cm_per_pixel));
    }
    
    template<DifferenceMethod method>
    BlobPtr _threshold(Blob& blob, int32_t value, const Background& background) {
        if(blob.pixels() == nullptr)
            throw U_EXCEPTION("Cannot threshold without pixel values.");
        
        auto lines = std::make_unique<std::vector<HorizontalLine>>();
        lines->reserve(blob.hor_lines().size());
        
        auto ptr = blob.pixels()->data();
        HorizontalLine tmp;
        auto tmp_pixels = std::make_unique<std::vector<uchar>>();
        tmp_pixels->reserve(blob.pixels()->size());
        
        for (auto &line : blob.hor_lines()) {
            tmp.x0 = line.x0;
            tmp.y = line.y;
            
            for (auto x=line.x0; x<=line.x1; ++x, ++ptr) {
                assert(ptr < blob.pixels()->data() + blob.pixels()->size());
                if(background.is_different<method>(x, line.y, *ptr, value)) {
                    tmp.x1 = x;
                    tmp_pixels->push_back(*ptr);
                    //blob._recount ++;
                    
                } else {
                    if(x > tmp.x0) {
                        lines->push_back(tmp);
                    }
                    tmp.x0 = x + 1;
                }
            }
            
            if(tmp.x1 == line.x1) {
                lines->push_back(tmp);
            }
        }
        
        return Blob::Make(
            std::move(lines),
            std::move(tmp_pixels),
            blob.flags(),
            blob::Prediction{blob.prediction()}
        );
    }

    BlobPtr Blob::threshold(int32_t value, const Background& background) {
        if(Background::enable_absolute_difference())
            return _threshold<DifferenceMethod::absolute>(*this, value, background);
        return _threshold<DifferenceMethod::sign>(*this, value, background);
    }
    
    std::tuple<Vec2, Image::Ptr> Blob::image(const cmn::Background* background, const Bounds& restricted, uchar padding) const {
        Bounds b(bounds().pos() - float(padding), bounds().size() + float(padding) * 2);
        if(background)
            b.restrict_to(background->bounds());
        else if(restricted.width > 0)
            b.restrict_to(restricted);
        else
            b.restrict_to(Bounds(0, 0, infinity<Float2_t>(), infinity<Float2_t>()));
        
        auto image = Image::Make(b.height, b.width);
        
        if(!background)
            std::fill(image->data(), image->data() + image->size(), uchar(0));
        else
            background->image().get()(b).copyTo(image->get());
        
        auto _x = (coord_t)b.x;
        auto _y = (coord_t)b.y;
        
        auto ptr = _pixels->data();
        for (auto &line : hor_lines()) {
            auto image_ptr = image->data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image->cols + ptr_safe_t(line.x0) - ptr_safe_t(_x));
            for (auto x=line.x0; x<=line.x1; ++x, ++ptr, ++image_ptr) {
                assert(ptr < _pixels->data() + _pixels->size());
                assert(image_ptr < image->data() + image->size());
                *image_ptr = *ptr;
            }
        }
        return {b.pos(), std::move(image)};
    }
    
    std::tuple<Vec2, Image::Ptr> Blob::alpha_image(const cmn::Background& background, int32_t threshold) const {
        Bounds b(bounds().pos()-Vec2(1), bounds().size()+Vec2(2));
        b.restrict_to(background.bounds());
        
        auto image = Image::Make(b.height, b.width, 4);
        std::fill(image->data(), image->data() + image->size(), uchar(0));
        
        auto _x = (coord_t)b.x;
        auto _y = (coord_t)b.y;
        
        int32_t value;
        float maximum = 0;
        auto ptr = _pixels->data();
        
        auto work = [&]<DifferenceMethod method>() {
            for (auto &line : hor_lines()) {
                //auto image_ptr = image->data() + ((line.y - _y) * image->cols * image->dims + (line.x0 - _x) * image->dims);
                auto image_ptr = image->data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image->cols + (ptr_safe_t(line.x0) - ptr_safe_t(_x))) * image->dims;
                for (auto x=line.x0; x<=line.x1; ++x, ++ptr, image_ptr += image->dims) {
                    assert(ptr < _pixels->data() + _pixels->size());
                    value = background.diff<method>(x, line.y, *ptr);
                    if(background.is_value_different(x, line.y, value, threshold)) {
                        if(maximum < value)
                            maximum = value;
                        *image_ptr = *(image_ptr+1) = *(image_ptr+2) = *ptr;
                        *(image_ptr+3) = value;
                    }
                }
            }
        };
        
        if(Background::enable_absolute_difference()) {
            work.operator()<DifferenceMethod::absolute>();
        } else {
            work.operator()<DifferenceMethod::sign>();
        }
        
        if(maximum > 0) {
            for (auto &line : hor_lines()) {
                auto image_ptr = image->data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image->cols + (ptr_safe_t(line.x0) - ptr_safe_t(_x))) * image->dims;
                for (auto x=line.x0; x<=line.x1; ++x, ++ptr, image_ptr += image->dims) {
                    *(image_ptr + 3) = min(255, float(*(image_ptr + 3)) / (maximum * 0.6) * 255);
                }
            }
        }
        
        return {b.pos(), std::move(image)};
    }

    std::tuple<Vec2, Image::Ptr> Blob::equalized_luminance_alpha_image(const cmn::Background& background, int32_t threshold, float minimum, float maximum) const {
        auto image = Image::Make();
        auto pos = equalized_luminance_alpha_image(background, threshold, minimum, maximum, *image);
        return { pos, std::move(image) };
    }
    Vec2 Blob::equalized_luminance_alpha_image(const cmn::Background& background, int32_t threshold, float minimum, float maximum, Image& image) const {
        Bounds b(bounds().pos()-Vec2(1), bounds().size()+Vec2(2));
        b.restrict_to(background.bounds());
        
        image.create(b.height, b.width, 2);
        std::fill(image.data(), image.data() + image.size(), uchar(0));
        
        auto work = [&]<DifferenceMethod method>(){
            auto _x = (coord_t)b.x;
            auto _y = (coord_t)b.y;
            
            minimum *= 0.5;
            
            float factor = 1;
            if(maximum > 0 && maximum != minimum)
                factor = 1.f / ((maximum - minimum) * 0.5) * 255;
            else
                minimum = 0;
            
            int32_t value;
            auto ptr = _pixels->data();
            
            for (auto &line : hor_lines()) {
                auto image_ptr = image.data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * ptr_safe_t(image.cols) * 2 + (ptr_safe_t(line.x0) - ptr_safe_t(_x)) * 2);
                for (auto x=line.x0; x<=line.x1; ++x, ++ptr, image_ptr+=2) {
                    assert(ptr < _pixels->data() + _pixels->size());
                    value = background.diff<method>(x, line.y, *ptr);
                    if(!threshold || background.is_value_different(x, line.y, value, threshold)) {
                        *image_ptr = saturate((float(*ptr) - minimum) * factor);
                        //*image_ptr = *ptr;
                        *(image_ptr+1) = saturate(int32_t(255 - SQR(1 - value / 255.0) * 255.0));
                    }
                }
            }
        };
        
        if(Background::enable_absolute_difference()) {
            work.operator()<DifferenceMethod::absolute>();
        } else {
            work.operator()<DifferenceMethod::sign>();
        }

        return b.pos();
    }

    cmn::Vec2 Blob::luminance_alpha_image(const cmn::Background& background, int32_t threshold, Image& image) const {
        Bounds b(bounds().pos() - Vec2(1), bounds().size() + Vec2(2));
        b.restrict_to(background.bounds());

        image.create(b.height, b.width, 2);
        std::fill(image.data(), image.data() + image.size(), uchar(0));

        auto work = [&]<DifferenceMethod method>(){
            auto _x = (coord_t)b.x;
            auto _y = (coord_t)b.y;
            
            int32_t value;
            auto ptr = _pixels->data();
            
            for (auto& line : hor_lines()) {
                auto image_ptr = image.data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image.cols * 2 + (ptr_safe_t(line.x0) - ptr_safe_t(_x)) * 2);
                for (auto x = line.x0; x <= line.x1; ++x, ++ptr, image_ptr += 2) {
                    assert(ptr < _pixels->data() + _pixels->size());
                    value = background.diff<method>(x, line.y, *ptr);
                    if (!threshold || background.is_value_different(x, line.y, value, threshold)) {
                        *image_ptr = *ptr;
                        *(image_ptr + 1) = saturate(int32_t(255 - SQR(1 - value / 255.0) * 255.0) * 2);
                    }
                }
            }
        };
        
        if(Background::enable_absolute_difference()) {
            work.operator()<DifferenceMethod::absolute>();
        } else {
            work.operator()<DifferenceMethod::sign>();
        }
        
        return b.pos();
    }
    std::tuple<Vec2, Image::Ptr> Blob::luminance_alpha_image(const cmn::Background& background, int32_t threshold) const {
        auto image = Image::Make();
        Vec2 pos = luminance_alpha_image(background, threshold, *image);
        return { pos, std::move(image) };
    }
    
    std::tuple<Vec2, Image::Ptr> Blob::difference_image(const cmn::Background& background, int32_t threshold) const {
        Bounds b(bounds().pos()-Vec2(1), bounds().size()+Vec2(2));
        b.restrict_to(background.bounds());
        
        auto image = Image::Make(b.height, b.width);
        std::fill(image->data(), image->data() + image->size(), uchar(0));
        
        auto _x = (coord_t)b.x;
        auto _y = (coord_t)b.y;
        
        
        auto work = [&]<DifferenceMethod method>(){
            int32_t value;
            auto ptr = _pixels->data();
            
            for (auto &line : hor_lines()) {
                auto image_ptr = image->data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image->cols + ptr_safe_t(line.x0) - ptr_safe_t(_x));
                for (auto x=line.x0; x<=line.x1; ++x, ++ptr, ++image_ptr) {
                    assert(ptr < _pixels->data() + _pixels->size());
                    value = background.diff<method>(x, line.y, *ptr);
                    if(!threshold || background.is_value_different(x, line.y, value, threshold))
                        *image_ptr = value;
                }
            }
        };
        
        if(Background::enable_absolute_difference()) {
            work.operator()<DifferenceMethod::absolute>();
        } else {
            work.operator()<DifferenceMethod::sign>();
        }
        
        return {b.pos(), std::move(image)};
    }
    
    void Blob::transfer_backgrounds(const cmn::Background &from, const cmn::Background &to, const Vec2& dest_offset) {
        auto ptr = (uchar*)_pixels->data();
        for (auto &line : hor_lines()) {
            for (auto x=line.x0; x<=line.x1; ++x, ++ptr) {
                assert(ptr < _pixels->data() + _pixels->size());
                *ptr = saturate(-int32_t(from.color(x, line.y)) + int32_t(*ptr) + to.color(ptr_safe_t(x) + dest_offset.x, ptr_safe_t(line.y) + dest_offset.y), 0, 255);
                //*ptr = saturate((int32_t)from.diff(x, line.y, *ptr) - to.color(x, line.y), 0, 255);
            }
        }
    }
    
    decltype(Blob::_pixels) Blob::calculate_pixels(const Image::Ptr& image, const decltype(_hor_lines) &lines, const Vec2& offset) {
        auto pixels = std::make_unique<std::vector<uchar>>();
        for(auto &line : *lines) {
            auto start = image->ptr(uint(int64_t(line.y) + int64_t(offset.y)), uint(int64_t(line.x0) + int64_t(offset.x)));
            //auto start = image->data() + (ptr_safe_t(line.y) + ptr_safe_t(offset.y)) * image->cols + (ptr_safe_t(line.x0) + ptr_safe_t(offset.x));
            auto end = start + ptr_safe_t(line.x1) - ptr_safe_t(line.x0) + 1;
            if(int64_t(line.x1) + int64_t(offset.x) < int64_t(image->cols)
               && int64_t(line.y) + int64_t(offset.y) < int64_t(image->rows)
               && int64_t(line.x0) + int64_t(offset.x) >= 0
               && int64_t(line.y) + int64_t(offset.y) >= 0)
            {
                pixels->insert(pixels->end(), start, end);
            } else {
#ifndef NDEBUG
                FormatExcept("line.x1 + offset.x = ", int64_t(line.x1) + int64_t(offset.x), " line.y + offset.y = ", int64_t(line.y) + int64_t(offset.y), " image:", image->bounds());
#endif
            }
        }
        return pixels;
    }
    
    std::tuple<Vec2, Image::Ptr> Blob::thresholded_image(const cmn::Background& background, int32_t threshold) const {
        Bounds b(bounds().pos()-Vec2(1), bounds().size()+Vec2(2));
        b.restrict_to(background.bounds());
        
        auto image = Image::Make(b.height, b.width);
        std::fill(image->data(), image->data() + image->size(), uchar(0));
        
        auto _x = (coord_t)b.x;
        auto _y = (coord_t)b.y;
        
        auto work = [&]<DifferenceMethod method>(){
            auto ptr = _pixels->data();
            for (auto &line : hor_lines()) {
                auto image_ptr = image->data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image->cols + ptr_safe_t(line.x0) - ptr_safe_t(_x));
                for (auto x=line.x0; x<=line.x1; ++x, ++ptr, ++image_ptr) {
                    assert(ptr < _pixels->data() + _pixels->size());
                    if(background.is_value_different(x, line.y, background.diff<method>(x, line.y, *ptr), threshold))
                        *image_ptr = *ptr;
                }
            }
        };
        
        if(Background::enable_absolute_difference()) {
            work.operator()<DifferenceMethod::absolute>();
        } else {
            work.operator()<DifferenceMethod::sign>();
        }
        
        return {b.pos(), std::move(image)};
    }
    
    std::tuple<Vec2, Image::Ptr> Blob::binary_image(const cmn::Background& background, int32_t threshold) const {
        Bounds b(bounds().pos()-Vec2(1), bounds().size()+Vec2(2));
        b.restrict_to(background.bounds());
        
        auto image = Image::Make(b.height, b.width);
        std::fill(image->data(), image->data() + image->size(), uchar(0));
        
        auto _x = (coord_t)b.x;
        auto _y = (coord_t)b.y;
        
        if(_pixels == nullptr)
            throw U_EXCEPTION("Cannot generate binary image without pixel values.");
        
        auto work = [&]<DifferenceMethod method>(){
            int32_t value;
            auto ptr = _pixels->data();
            for (auto &line : hor_lines()) {
                auto image_ptr = image->data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image->cols + ptr_safe_t(line.x0) - ptr_safe_t(_x));
                for (auto x=line.x0; x<=line.x1; ++x, ++ptr, ++image_ptr) {
                    assert(ptr < _pixels->data() + _pixels->size());
                    value = background.diff<method>(x, line.y, *ptr);
                    if(background.is_value_different(x, line.y, value, threshold))
                        *image_ptr = 255;
                }
            }
        };
        
        if(Background::enable_absolute_difference()) {
            work.operator()<DifferenceMethod::absolute>();
        } else {
            work.operator()<DifferenceMethod::sign>();
        }
        
        return {b.pos(), std::move(image)};
    }
    
    std::tuple<Vec2, Image::Ptr> Blob::binary_image() const {
        Bounds b(bounds().pos()-Vec2(1), bounds().size()+Vec2(2));
        if(b.x < 0) {b.x = 0;--b.width;}
        if(b.y < 0) {b.y = 0;--b.height;}
        
        auto image = Image::Make(b.height, b.width);
        std::fill(image->data(), image->data() + image->size(), uchar(0));
        
        auto _x = (coord_t)b.x;
        auto _y = (coord_t)b.y;
        
        for (auto &line : hor_lines()) {
            auto image_ptr = image->data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image->cols + ptr_safe_t(line.x0) - ptr_safe_t(_x));
            for (auto x=line.x0; x<=line.x1; ++x, ++image_ptr) {
                *image_ptr = 255;
            }
        }
        return {b.pos(), std::move(image)};
    }
    
    void Blob::set_pixels(pixel_ptr_t&& pixels) {
        _pixels = std::move(pixels);
        assert(!_pixels || _pixels->size() == num_pixels());
    }
    
/*void Blob::set_pixels(const cmn::grid::PixelGrid &grid, const cmn::Vec2& offset) {
    throw U_EXCEPTION("Deprecation.");
        auto pixels = std::make_shared<std::vector<uchar>>();
        for (auto &line : hor_lines()) {
            auto current = pixels->size();
            pixels->resize(pixels->size() + line.x1 - line.x0 + 1);
            grid.copy_row(line.x0 - offset.x, line.x1 - offset.x, line.y - offset.y, pixels->data() + current);
            //for (ushort x=line.x0; x<=line.x1; ++x) {
                //assert(ptr < _pixels->data() + _pixels->size());
              //  pixels->push_back(grid.query(x, line.y));
            //}
        }
        _pixels = pixels;
    }*/

    std::string Blob::name() const {
        auto center = bounds().pos() + bounds().size() * 0.5;
        auto id = blob_id();
        //auto x = id >> 16;
        //auto y = id & 0x0000FFFF;
        return Meta::toStr(id)+" ["+dec<2>(center.x).toStr()+","+dec<2>(center.y).toStr()+"]";// * setting(cm_per_pixel));
    }
    
    void Blob::add_offset(const cmn::Vec2 &off) {
        if(off == Vec2(0))
            return;
        
        int offy = off.y;
        int offx = off.x;
        if(!_hor_lines->empty() && offy < -float(bounds().y)) {
            offy = -float(bounds().y);
        }
        if(!_hor_lines->empty() && offx < -float(bounds().x)) {
            offx = -float(bounds().x);
        }
        
        for (auto &h : *_hor_lines) {
            h.y += offy;
            h.x0 += offx;
            h.x1 += offx;
        }
        
        if(_properties.ready) {
            _properties.ready = false;
            calculate_properties();
        }
        if(_moments.ready) {
            _moments.ready = false;
            calculate_moments();
        }
        
        //auto center = bounds().pos() + bounds().size() * 0.5;
        _blob_id = bid::from_blob(*this);
    }
    
    void Blob::scale_coordinates(const cmn::Vec2 &scale) {
        auto center = bounds().pos() + bounds().size() * 0.5;
        Vec2 offset = center.mul(scale) - center;
        
        add_offset(offset);
    }

    size_t Blob::memory_size() const {
        size_t overall = sizeof(Blob);
        if(_pixels)
            overall += _pixels->size() * sizeof(uchar);
        overall += _hor_lines->size() * sizeof(decltype(_hor_lines)::element_type::value_type);
        return overall;
    }
    
    std::string Blob::toStr() const {
        return "pv::Blob<" + Meta::toStr(blob_id()) + " " + Meta::toStr(bounds().pos() + bounds().size() * 0.5) + " " + Meta::toStr(_pixels ? _pixels->size() * SQR(setting(cm_per_pixel)) : -1) + ">";
    }
}
