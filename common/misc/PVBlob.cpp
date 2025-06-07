#include <misc/PVBlob.h>
#include <misc/GlobalSettings.h>
#include <misc/Timer.h>
#include <misc/create_struct.h>
#include <misc/ThreadPool.h>

#if defined(USE_NEON)
    #include <arm_neon.h>
#elif defined(USE_SSE)
    #include <emmintrin.h>
    #include <smmintrin.h>
    #include <immintrin.h>
    #if defined(_MSC_VER)
        #include <intrin.h>
    #endif
#endif

namespace pv {
    
    using namespace cmn;
    using namespace cmn::blob;

    cmn::blob::line_ptr_t LineMaker::operator()() const {
        /*if(buffers().size() % 1000u == 0) {
            thread_print("buffer size = ", buffers().size());
        }*/
        return std::make_unique<cmn::blob::lines_t>((NoInitializeAllocator<cmn::blob::lines_t::value_type>()));
    }

    buffers_t& buffers() {
        static buffers_t* instance = new buffers_t;
        return *instance; /// no delete in order to avoid cxx_finalize exception
    }

    // Template function to call a lambda with a compile-time constant index
    template<typename Lambda, auto Index>
    constexpr void callLambdaWithIndex(Lambda&& lambda) {
        lambda.template operator() < Index > ();
    }

    // Helper structure for compile-time recursion
    template<int Start, int End>
    struct LambdaCaller {
        template<typename Lambda>
        static constexpr void call(Lambda&& lambda) {
            callLambdaWithIndex<Lambda, Start>(lambda);
            LambdaCaller<Start + 1, End>::call(std::forward<Lambda>(lambda));
        }
    };

    // Base case for recursion
    template<int End>
    struct LambdaCaller<End, End> {
        template<typename Lambda>
        static constexpr void call(Lambda&&) {}
    };

CREATE_STRUCT(PVSettings,
    (Float2_t, cm_per_pixel),
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
    if(_hor_lines) {
        pv::buffers().move_back(std::move(_hor_lines));
    }
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
    if (_moments.ready)
        return;
    
    assert(_properties.ready);

    static GenericThreadPool pool(8, "moments_pool");
    const uint32_t num_threads = _hor_lines->size() > 1000 ? min(narrow_cast<uint32_t>(_hor_lines->size()), 4u) : 1;
    // Create a vector of _moments copies for each thread
    std::vector<Moments> thread_moments(num_threads, Moments{});

    // Parallel processing of the first loop
    //fn(i, it, nex, conditional_conversion<4, size_t, int64_t, Iterator, Iterator, size_t>::template convert<std::remove_cvref_t<F>>(j));
    distribute_indexes([&](auto, auto start, auto end, int64_t thread_index) {
        Moments& local_moments = thread_moments.at(thread_index);
        assert(thread_index < num_threads);
    
        for(auto it = start; it != end; ++it) {
            auto& h = *it;
            const uint my = h.y;
            const uint mysq = my * my;
            
            int mx = h.x0;
            
            for (int x = h.x0; x <= h.x1; x++, mx++) {
                const uint mxsq = mx * mx;
                
                local_moments.m[0][0] += 1;
                local_moments.m[0][1] += 1 * my;
                local_moments.m[1][0] += mx * 1;
                local_moments.m[1][1] += mx * my;
                
                local_moments.m[2][0] += mxsq;
                local_moments.m[0][2] += mysq;
                local_moments.m[2][1] += mxsq * my;
                local_moments.m[1][2] += mx * mysq;
                local_moments.m[2][2] += mxsq * mysq;
            }
        }
    }, pool, _hor_lines->begin(), _hor_lines->end(), num_threads);
    
    // Merge thread-local moments into the main _moments structure
    for (const auto& local_moments : thread_moments) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                _moments.m[i][j] += local_moments.m[i][j];
            }
        }
    }

    _properties.center = Vec2(_moments.m[1][0] / _moments.m[0][0],
                              _moments.m[0][1] / _moments.m[0][0]);
    
    assert(not cmn::isnan(_properties.center.x)
           && not cmn::isnan(_properties.center.y));

    // Parallel processing of the second loop
    distribute_indexes([&](auto, auto start, auto end, int64_t thread_index) {
        Moments& local_moments = thread_moments.at(thread_index);

        for(auto it = start; it != end; ++it) {
            auto& h = *it;
            const int vy = (h.y) - _properties.center.y;
            const int vy2 = vy * vy;
            
            int vx = (h.x0) - _properties.center.x;
            
            for (int x = h.x0; x <= h.x1; x++, vx++) {
                const auto vx2 = vx * vx;
                
                local_moments.mu[0][0] += 1;
                local_moments.mu[0][1] += 1 * vy;
                local_moments.mu[0][2] += 1 * vy2;
                
                local_moments.mu[1][0] += vx * 1;
                local_moments.mu[1][1] += float(vx) * float(vy);
                local_moments.mu[1][2] += float(vx) * float(vy2);
                
                local_moments.mu[2][0] += vx2 * 1;
                local_moments.mu[2][1] += float(vx2) * float(vy);
                local_moments.mu[2][2] += float(vx2) * float(vy2);
            }
        }
    }, pool, _hor_lines->begin(), _hor_lines->end(), num_threads);

    // Merge thread-local moments into the main _moments structure
    for (const auto& local_moments : thread_moments) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                _moments.mu[i][j] += local_moments.mu[i][j];
            }
        }
    }

    float mu00_inv = 1.0f / float(_moments.mu[0][0]);
    for (int i = 0; i<3; i++) {
        for (int j = 0; j<3; j++) {
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
    int max_x = 0, height = 0, min_x = _lines.empty() ? 0 : infinity<int>();
    int x0, x1;
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
    auto flines = pv::buffers().get(source_location::current());
    ShortHorizontalLine::uncompress(*flines, start_y, _lines);
    
    auto ptr = pv::Blob::Make(std::move(flines), nullptr, 0, blob::Prediction{pred});
    ptr->set_parent_id((status_byte & 0x2) != 0 ? parent_id : pv::bid::invalid);
    
    bool tried_to_split = (status_byte & 0x4) != 0;
    ptr->set_tried_to_split(tried_to_split);
    
    if((status_byte & 0x1) != 0 && (status_byte & 0x2) == 0) {
        ptr->set_split(true);
    } else
        ptr->set_split(false);
    
    ptr->set_tag(is_tag());
    ptr->set_instance_segmentation(is_instance_segmentation());
    ptr->set_rgb(is_rgb());
    ptr->set_r3g3b2(is_r3g3b2());
    ptr->set_binary(is_binary());
    
    return ptr;
}


    std::vector<ShortHorizontalLine>
        ShortHorizontalLine::compress(const std::vector<HorizontalLine>& lines)
    {
        if(lines.empty())
            return {};
        
//#if defined(USE_NEON)
        std::vector<pv::ShortHorizontalLine> ret((NoInitializeAllocator<pv::ShortHorizontalLine>()));
        ret.resize(lines.size());
        
        auto start = lines.data(), end = lines.data() + lines.size();
        auto rptr = ret.data();
        auto prev_y = ret.empty() ? 0 : lines.front().y;
        *rptr = pv::ShortHorizontalLine(start->x0, start->x1);
        rptr++;
        
        for(auto lptr = start + 1; lptr != end; lptr++, rptr++) {
            *rptr = pv::ShortHorizontalLine(lptr->x0, lptr->x1);
            
            //if(prev_y != lptr->y)
                (rptr-1)->eol(prev_y != lptr->y);
            prev_y = lptr->y;
        }

        return ret;
/*#else
        std::vector<ShortHorizontalLine> ret((NoInitializeAllocator<pv::ShortHorizontalLine>()));
        ret.resize(lines.size());

        // Pointers to the start and end of the input and output vectors
        auto lptr = reinterpret_cast<const HorizontalLine*>(lines.data());
        auto end = lptr + lines.size(); // NEON processes 8 elements per loop
        auto rptr = reinterpret_cast<ShortHorizontalLine*>(ret.data());

        // Initialize previous y to the first element's y, or 0 if empty
        uint16_t prev_y = lines.empty() ? 0 : lines.front().y;

        // NEON constants
        const uint16x8_t x_mask = vdupq_n_u16(0x7FFF); // Mask for x values
        const uint16x8_t eol_mask = vdupq_n_u16(0x8000); // Mask for the EOL flag

        for (; lptr + 8 <= end; lptr += 8, rptr += 8) {
            // Load 8 HorizontalLines at once
            uint16_t next_y;
            if(lptr + 8 < end)
                next_y = (lptr + 8)->y;
            else
                next_y = -1;
            //printf("Next y = %u\n", next_y);
            
            uint16x8x4_t data_vec = vld4q_u16(reinterpret_cast<const uint16_t*>(lptr));
            //printf("x0 = {0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X}\n", vgetq_lane_u16(data_vec.val[0], 0), vgetq_lane_u16(data_vec.val[0], 1), vgetq_lane_u16(data_vec.val[0], 2), vgetq_lane_u16(data_vec.val[0], 3), vgetq_lane_u16(data_vec.val[0], 4), vgetq_lane_u16(data_vec.val[0], 5), vgetq_lane_u16(data_vec.val[0], 6), vgetq_lane_u16(data_vec.val[0], 7));

            // Extract x0, x1, and y values
            uint16x8_t x0_vec = data_vec.val[0];
            //printf("x0  = {0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X}\n", vgetq_lane_u16(x0_vec, 0), vgetq_lane_u16(x0_vec, 1), vgetq_lane_u16(x0_vec, 2), vgetq_lane_u16(x0_vec, 3), vgetq_lane_u16(x0_vec, 4), vgetq_lane_u16(x0_vec, 5), vgetq_lane_u16(x0_vec, 6), vgetq_lane_u16(x0_vec, 7));

            uint16x8_t x1_vec = vandq_u16(data_vec.val[1], x_mask);
            //printf("x1  = {0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X}\n", vgetq_lane_u16(x1_vec, 0), vgetq_lane_u16(x1_vec, 1), vgetq_lane_u16(x1_vec, 2), vgetq_lane_u16(x1_vec, 3), vgetq_lane_u16(x1_vec, 4), vgetq_lane_u16(x1_vec, 5), vgetq_lane_u16(x1_vec, 6), vgetq_lane_u16(x1_vec, 7));

            uint16x8_t y_vec = data_vec.val[2];
            //printf("y   = {0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X}\n", vgetq_lane_u16(y_vec, 0), vgetq_lane_u16(y_vec, 1), vgetq_lane_u16(y_vec, 2), vgetq_lane_u16(y_vec, 3), vgetq_lane_u16(y_vec, 4), vgetq_lane_u16(y_vec, 5), vgetq_lane_u16(y_vec, 6), vgetq_lane_u16(y_vec, 7));

            uint16x8_t shifted_y_vec = vextq_u16(y_vec, vdupq_n_u16(next_y), 1);
            //printf("y'  = {0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X}\n", vgetq_lane_u16(shifted_y_vec, 0), vgetq_lane_u16(shifted_y_vec, 1), vgetq_lane_u16(shifted_y_vec, 2), vgetq_lane_u16(shifted_y_vec, 3), vgetq_lane_u16(shifted_y_vec, 4), vgetq_lane_u16(shifted_y_vec, 5), vgetq_lane_u16(shifted_y_vec, 6), vgetq_lane_u16(shifted_y_vec, 7));

            // Calculate EOL flags based on changes in y
            uint16x8_t eol_flags = vceqq_u16(y_vec, shifted_y_vec);
            //uint16x8_t eol_flags = vceqq_u16(y_vec, vdupq_n_u16(prev_y));
            //printf("eol = {0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X}\n", vgetq_lane_u16(eol_flags, 0), vgetq_lane_u16(eol_flags, 1), vgetq_lane_u16(eol_flags, 2), vgetq_lane_u16(eol_flags, 3), vgetq_lane_u16(eol_flags, 4), vgetq_lane_u16(eol_flags, 5), vgetq_lane_u16(eol_flags, 6), vgetq_lane_u16(eol_flags, 7));
            
            eol_flags = vmvnq_u16(eol_flags); // Invert the EOL flags (=> 0xFFFF where y changes)
            //eol_flags = vshrq_n_u16(eol_flags, 15); // Shift to make it a flag
            eol_flags = vandq_u16(eol_flags, eol_mask);

            //printf("eol = {0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X}\n", vgetq_lane_u16(eol_flags, 0), vgetq_lane_u16(eol_flags, 1), vgetq_lane_u16(eol_flags, 2), vgetq_lane_u16(eol_flags, 3), vgetq_lane_u16(eol_flags, 4), vgetq_lane_u16(eol_flags, 5), vgetq_lane_u16(eol_flags, 6), vgetq_lane_u16(eol_flags, 7));

            // Combine x1 and EOL flags
            x1_vec = vorrq_u16(x1_vec, eol_flags);
            //printf("x1' = {0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X}\n", vgetq_lane_u16(x1_vec, 0), vgetq_lane_u16(x1_vec, 1), vgetq_lane_u16(x1_vec, 2), vgetq_lane_u16(x1_vec, 3), vgetq_lane_u16(x1_vec, 4), vgetq_lane_u16(x1_vec, 5), vgetq_lane_u16(x1_vec, 6), vgetq_lane_u16(x1_vec, 7));

            // extract EOL flags back from x1 for testing:
            //uint16x8_t eol_flags2 = vandq_u16(x1_vec, eol_mask);
            //eol_flags2 = vshrq_n_u16(eol_flags2, 15);
            //printf("eol2= {0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X}\n", vgetq_lane_u16(eol_flags2, 0), vgetq_lane_u16(eol_flags2, 1), vgetq_lane_u16(eol_flags2, 2), vgetq_lane_u16(eol_flags2, 3), vgetq_lane_u16(eol_flags2, 4), vgetq_lane_u16(eol_flags2, 5), vgetq_lane_u16(eol_flags2, 6), vgetq_lane_u16(eol_flags2, 7));

            uint16x8x2_t tmp = vzipq_u16(x0_vec, x1_vec);
            //printf("tmp[0,0:8] = {0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X}\n", vgetq_lane_u16(tmp.val[0], 0), vgetq_lane_u16(tmp.val[0], 1), vgetq_lane_u16(tmp.val[0], 2), vgetq_lane_u16(tmp.val[0], 3), vgetq_lane_u16(tmp.val[0], 4), vgetq_lane_u16(tmp.val[0], 5), vgetq_lane_u16(tmp.val[0], 6), vgetq_lane_u16(tmp.val[0], 7));

            // Store the results
            vst1q_u16(reinterpret_cast<uint16_t*>(rptr), tmp.val[0]);
            vst1q_u16(reinterpret_cast<uint16_t*>(rptr) + 8, tmp.val[1]);

            // Update prev_y for the next iteration
            prev_y = (lptr + 7)->y; // Last y value in the current batch
            //printf("\n");

            //printf("i = %lu\n", std::distance(lines.data(), lptr) + 8);
        }

        // Handle remaining elements
        for (; lptr < end; ++lptr, ++rptr) {
            //printf("Manual %d\n", std::distance(lines.data(), lptr));
            *rptr = ShortHorizontalLine(lptr->x0, lptr->x1);
            if (prev_y != lptr->y) {
                (rptr - 1)->eol(true);
            }
            prev_y = lptr->y;
            ++lptr;
        }

        ret.back().eol(false);

        return ret;
#endif*/
    }

#ifdef USE_NEON
    __attribute__((target("neon"))) // NEON is required for vld1q_u32
    //__attribute__((noinline))
    void ShortHorizontalLine::uncompress(
            std::vector<cmn::HorizontalLine>& _result,
            uint16_t start_y,
            const std::vector<ShortHorizontalLine>& compressed) noexcept // Assuming compressed is a vector of uint32_t
    {
        //uncompress_normal(_result, start_y, compressed);
        //return;
        
        auto result = &_result;
        result->resize(compressed.size());

        auto y = start_y;
        auto uptr = result->data();
        auto cptr = reinterpret_cast<const uint32_t*>(compressed.data());
        auto end = cptr + compressed.size() - (compressed.size() % 4); // NEON processes 4 elements per loop with 128-bit vectors

        uint32x4_t x_mask = vdupq_n_u32(0x7FFFFFFF);
        uint32x4_t eol_mask = vdupq_n_u32(0x80000000);
        uint32x4_t zeros = vmovq_n_u32(0);

        static_assert(sizeof(ShortHorizontalLine) == 4, "ShortHorizontalLine is not 4 bytes");
        static_assert(sizeof(HorizontalLine) == sizeof(uint64_t), "HorizontalLine is not 8 bytes");

        for (; cptr < end; cptr += 4, uptr += 4) {
            uint32x4_t data_vec = vld1q_u32(cptr);
                    //printf("Loaded data_vec: {0x%X, 0x%X, 0x%X, 0x%X}\n", vgetq_lane_u32(data_vec, 0), vgetq_lane_u32(data_vec, 1), vgetq_lane_u32(data_vec, 2), vgetq_lane_u32(data_vec, 3));

            uint32x4_t X = vandq_u32(data_vec, x_mask);
            //printf("X coordinates: {0x%X, 0x%X, 0x%X, 0x%X}\n", vgetq_lane_u32(X, 0), vgetq_lane_u32(X, 1), vgetq_lane_u32(X, 2), vgetq_lane_u32(X, 3));

            // Correcting the Low part (x0) extraction:
            // Narrow the 32-bit values in X to 16-bit values to extract the lower half (x0).
            // This should properly extract the lower 16 bits.
            uint16x4_t low_part = vmovn_u32(X);
            //printf("A: {0x%X, 0x%X, 0x%X, 0x%X}\n", vget_lane_u16(low_part, 0), vget_lane_u16(low_part, 1), vget_lane_u16(low_part, 2), vget_lane_u16(low_part, 3));
            
            // Correcting the High part (x1) extraction:
            // First, shift the data right by 16 bits to position the x1 part in the lower 16 bits.
            // Then, apply a mask to ensure only the lower 15 bits are retained (assuming the 16th bit is not part of x1).
            // Finally, narrow the result to a 16-bit value.
            uint32x4_t high_bits_shifted = vshrq_n_u32(X, 16);
            uint32x4_t high_mask = vdupq_n_u32(0x7FFF); // 15-bit mask for x1.
            uint16x4_t high_part = vmovn_u32(vandq_u32(high_bits_shifted, high_mask));

            uint32x4_t eol_flags = vshrq_n_u32(vandq_u32(data_vec, eol_mask), 31);
            uint16x4_t y_values = vdup_n_u16(y); // Update y_values based on the new y
            coord_t y_increment = vaddvq_u32(eol_flags);// Prepare the y and padding values for all elements

            auto accum = [&]<int i>() {
                // Shift-right eol_flags by 1 bit and add the remaining bits to y_values
                // Shift elements to the left and introduce zeros from the right
                eol_flags = vextq_u32(zeros, eol_flags, 3);
                y_values = vmovn_u32(vaddq_u32(vmovl_u16(y_values), eol_flags));
            };

            LambdaCaller<0, 3>::call(accum);

            uint16x4_t A = low_part;
            uint16x4_t B = high_part;
            uint16x4_t C = y_values;
            uint16x4_t D = vdup_n_u16(0);

            // Step 1: Combine A and B
            uint16x8_t tempAB = vcombine_u16(A, B); // Combine A and B into a single 8x16 vector
            uint16x4x2_t zippedAB = vzip_u16(vget_low_u16(tempAB), vget_high_u16(tempAB));

            // Step 2: Combine C and D
            uint16x8_t tempCD = vcombine_u16(C, D); // Combine C and D into a single 8x16 vector
            uint16x4x2_t zippedCD = vzip_u16(vget_low_u16(tempCD), vget_high_u16(tempCD));
            // Now, zippedCD.val[0] contains C0, D0, C1, D1
            // And zippedCD.val[1] contains C2, D2, C3, D3

            // Step 3: Create v0 and v1
            uint16x8_t combinedAB0 = vcombine_u16(zippedAB.val[0], zippedAB.val[1]);
            uint16x8_t combinedCD0 = vcombine_u16(zippedCD.val[0], zippedCD.val[1]);

            uint32x4x2_t interleaved1 = vzipq_u32(vreinterpretq_u32_u16(combinedAB0), vreinterpretq_u32_u16(combinedCD0));
            uint16x8_t interleaved2 = vreinterpretq_u16_u32(interleaved1.val[0]);
            uint16x8_t interleaved3 = vreinterpretq_u16_u32(interleaved1.val[1]);
            uint16x8x2_t _interleaved;
            _interleaved.val[0] = interleaved2;
            _interleaved.val[1] = interleaved3;
            
            // Step 4: Store the interleaved results as 64-bit values in pairs of two
            uint64x2_t interleaved4 = vreinterpretq_u64_u16(_interleaved.val[0]);
            uint64x2_t interleaved5 = vreinterpretq_u64_u16(_interleaved.val[1]);

            vst1q_u64((uint64_t*)uptr, interleaved4);
            vst1q_u64((uint64_t*)(uptr + 2), interleaved5);

            y += y_increment;
        }

        // Process the remaining elements (if any)
        for (auto remaining_end = cptr + (compressed.size() % 4); cptr < remaining_end; ++cptr, ++uptr) {
            //std::cout << "Processing remaining element y=" << y << "\n";
            uint32_t data = *cptr;
            uptr->y = y;
            uptr->x0 = uint16_t(data & 0xFFFF);
            uptr->x1 = uint16_t((data >> 16) & 0x7FFF);
            if (data & 0x80000000) y++;
        }
        
        /*auto result2 = uncompress_normal(start_y, compressed);
        assert(result->size() == result2->size());
        for(size_t i=0; i<result->size(); ++i) {
            assert((*result)[i] == (*result2)[i]);
        }*/

        //return result;
    }

    // NEON-specific implementation
#elif defined(USE_SSE)

#if defined(_MSC_VER)
    bool check_cpu_features() {
        int cpuInfo[4] = {};
        __cpuid(cpuInfo, 0);

        if (cpuInfo[0] >= 7) {
            __cpuidex(cpuInfo, 7, 0);
            return (cpuInfo[1] & (1 << 16)) != 0;  // Check for AVX-512F support
        }

        return false;
    }
    static bool use_avx512f = check_cpu_features();
#endif

#if !defined(_MSC_VER)
    __attribute__((target("avx512f"))) // AVX512F is required for _mm512_setr_epi64
#endif
    void ShortHorizontalLine::uncompress(
            std::vector<cmn::HorizontalLine>& result,
            uint16_t start_y,
            const std::vector<ShortHorizontalLine>& compressed) 
        noexcept // Assuming compressed is a vector of uint32_t
    {
#if defined(_MSC_VER)
        if (not use_avx512f) {
            static std::once_flag print_flag;
            std::call_once(print_flag, []() {
                std::cout << "[AVX512] Using fallback instruction set." << std::endl;
            });
            uncompress_normal(result, start_y, compressed);
            return;
        }
#endif

        result.resize(compressed.size());

        auto y = start_y;
        auto uptr = result.data();
        auto cptr = reinterpret_cast<const __m256i*>(compressed.data());
        auto end = reinterpret_cast<const __m256i*>(compressed.data() + compressed.size() - compressed.size() % 8u); // Processing 4 elements per loop with 256-bit vectors

        const __m256i x_mask = _mm256_set1_epi32(0x7FFFFFFF);
        const __m256i eol_mask = _mm256_set1_epi32(0x80000000);
        const __m512i zeros = _mm512_setzero_si512();

        static_assert(sizeof(ShortHorizontalLine) == 4, "ShortHorizontalLine is not 4 bytes");
        static_assert(sizeof(HorizontalLine) == sizeof(uint64_t), "HorizontalLine is not 8 bytes");

        for (; cptr < end; ++cptr, uptr += 8) {
            __m256i data_vec = _mm256_loadu_si256(cptr);

            // Extract x0 and x1, interleave with zeros to 512 bits
            __m256i X = _mm256_and_si256(data_vec, x_mask);
            __m512i X_512 = _mm512_zextsi256_si512(X);

            __m512i low_part = _mm512_unpacklo_epi32(X_512, zeros);
            __m512i high_part = _mm512_unpackhi_epi32(X_512, zeros);

            // Now interleave low_part and high_part correctly
            __m512i interleaved = _mm512_permutex2var_epi64(low_part, _mm512_setr_epi64(0, 1, 8, 9, 2, 3, 10, 11), high_part);

            // Extract eol flags
            __m256i eol_flags = _mm256_srli_epi32(_mm256_and_si256(data_vec, eol_mask), 31);
            _mm512_storeu_si512(reinterpret_cast<void*>(uptr), interleaved);

#if defined(__GNUC__)
            uptr[0].y = y; y += _mm256_extract_epi32(eol_flags, 0);
            uptr[1].y = y; y += _mm256_extract_epi32(eol_flags, 1);
            uptr[2].y = y; y += _mm256_extract_epi32(eol_flags, 2);
            uptr[3].y = y; y += _mm256_extract_epi32(eol_flags, 3);
            uptr[4].y = y; y += _mm256_extract_epi32(eol_flags, 4);
            uptr[5].y = y; y += _mm256_extract_epi32(eol_flags, 5);
            uptr[6].y = y; y += _mm256_extract_epi32(eol_flags, 6);
            uptr[7].y = y; y += _mm256_extract_epi32(eol_flags, 7);
#else
            auto runC = [&]<int i>() {
                uptr[i].y = y;
                y += _mm256_extract_epi32(eol_flags, i % 8);
            };

            LambdaCaller<0, 8>::call(runC);
#endif
        }

        // Process the remaining elements (if any)
        auto remaining_ptr = reinterpret_cast<const uint32_t*>(cptr);
        for (auto remaining_end = remaining_ptr + (compressed.size() % 8); remaining_ptr < remaining_end; ++remaining_ptr, ++uptr) {
            uint32_t data = *remaining_ptr;
            uptr->y = y;
            uptr->x0 = uint16_t(data & 0xFFFF);
            uptr->x1 = uint16_t((data >> 16) & 0x7FFF);
            if (data & 0x80000000) y++;
        }
    }

#endif

#if defined(USE_NEON) || defined(_MSC_VER)
    void ShortHorizontalLine::uncompress_normal(
        std::vector<cmn::HorizontalLine>& _result,
        uint16_t start_y,
        const std::vector<ShortHorizontalLine>& compressed) noexcept
#else
#if defined(USE_SSE)
    __attribute__((target("default")))
#endif
    void ShortHorizontalLine::uncompress(
            std::vector<cmn::HorizontalLine>& _result,
            uint16_t start_y,
            const std::vector<ShortHorizontalLine>& compressed) noexcept
#endif
    {
        _result.resize(compressed.size());
        
        auto y = start_y;
        auto uptr = _result.data();
        auto cptr = compressed.data(), end = compressed.data()+compressed.size();
        
        for(; cptr != end; cptr++, uptr++) {
            uptr->y = y;
            uptr->x0 = cptr->x0();
            uptr->x1 = cptr->x1();
            
            if(cptr->eol())
                y++;
        }
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
        assert(not _pixels || _pixels->size() == channels() * num_pixels());
        if(is_tag())
            Print("This blob is a tag ", *this);
    }

    Blob::Blob(line_ptr_t&& lines, uint8_t flags, Prediction&& pred)
        : _hor_lines(std::move(lines)), _pixels(nullptr), _prediction(std::move(pred))
    {
    #ifdef _DEBUG_MEMORY
        std::lock_guard<std::mutex> guard(all_mutex_blob);
        all_blobs_map[this] = retrieve_stacktrace();
    #endif
        
        if(!_hor_lines)
            throw U_EXCEPTION("Blob initialized with NULL lines array");
        
        init();
        _flags |= flags;
        assert(not _pixels || _pixels->size() == channels() * num_pixels());
        if(is_tag())
            Print("This blob is a tag ", *this);
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
            set_instance_segmentation(parent->is_instance_segmentation());
            set_rgb(parent->is_rgb());
            set_r3g3b2(parent->is_r3g3b2());
            set_binary(parent->is_binary());
        } else
            _parent_id = bid::invalid;
        
        if(!_parent_id.valid() && split)
            Print("Parent has to be set correctly in order to split blobs (",blob_id(),").");
    }
    
    void Blob::set_parent_id(const bid& parent_id) {
        set_split(parent_id.valid());
        _parent_id = parent_id;
    }
    
    void Blob::force_set_recount(int32_t threshold, float value) {
        if(threshold && _recount_threshold == threshold) {
            //Print("Not forcing recount of ",blob_id()," because it has already been calculated.");
            return;
        }
        
        _recount = (value != -1 ? value : num_pixels());
        _recount_threshold = threshold;
    }

    std::tuple<OutputInfo, std::unique_ptr<std::vector<uchar>>> Blob::calculate_pixels(const cv::Mat& average) const
    {
        InputInfo input{
            .channels = static_cast<uint8_t>(average.channels()),
            .encoding = average.channels() == 1 ? meta_encoding_t::gray : meta_encoding_t::rgb8
        };
        
        OutputInfo output;
        output = input_info();
        
        if(output.encoding == meta_encoding_t::binary)
            return {output, nullptr};
        return std::make_tuple(output, calculate_pixels(input, output, average));
    }

std::unique_ptr<std::vector<uchar>> Blob::calculate_pixels(InputInfo input, OutputInfo output, const cv::Mat& average) const
{
    if(empty())
        return nullptr;
    return calculate_pixels(input, output, *_hor_lines, average, num_pixels());
}

std::unique_ptr<std::vector<uchar>> Blob::calculate_pixels(cmn::InputInfo input, cmn::OutputInfo output, const std::vector<HorizontalLine>& lines, const cv::Mat& average, std::optional<uint64_t> num_pixels)
{
    assert(input.channels == average.channels());
    
    if(output.encoding == meta_encoding_t::binary)
        return nullptr;
    
    auto res = std::make_unique<std::vector<uchar>>();
    if(not num_pixels) {
        uint64_t N = 0;
        for(auto& line : lines) {
            N += line.length();
        }
        num_pixels = N;
    }
    
    res->resize(num_pixels.value() * output.channels);
    
    auto ptr = res->data();
    
    call_image_mode_function(input, output, [&]<InputInfo input, OutputInfo output, DifferenceMethod>() {
        static_assert(is_in(input.channels, 0, 1, 3), "Only 1 or 3 channels input is supported.");
        static_assert(is_in(output.channels, 1,3), "Only 1 or 3 channels output is supported.");
        
        for(auto &hl : lines) {
            for (ushort x=hl.x0; x<=hl.x1; ++x, ptr += output.channels) {
                assert(ptr < res->data() + res->size());
                if(ptr >= res->data() + res->size())
                    throw U_EXCEPTION("ptr is larger than ", res->size(),".");
                auto value = diffable_pixel_value<input, output>(average.ptr(hl.y, x));
                
                if constexpr(output.channels == 1) {
                    *ptr = value;
                    
                } else if constexpr(output.channels == 3) {
                    *(ptr + 0) = value[0];
                    *(ptr + 1) = value[1];
                    *(ptr + 2) = value[2];
                }
            }
        }
    });
    
    return res;
}

    std::tuple<OutputInfo, std::unique_ptr<std::vector<uchar>>> Blob::calculate_pixels(const cmn::Background& background) const
    {
        InputInfo input = input_info();
        OutputInfo output;
        output = input;
        
        auto res = std::make_unique<std::vector<uchar>>();
        res->resize(num_pixels() * output.channels);
        auto ptr = res->data();
        
        call_image_mode_function(input, output, [&]<InputInfo input, OutputInfo output, DifferenceMethod>() {
            static_assert(is_in(input.channels, 0, 1, 3), "Only 0, 1 or 3 channels input is supported.");
            static_assert(is_in(output.channels, 1,3), "Only 1 or 3 channels output is supported.");
            
            for(auto &hl : *_hor_lines) {
                for (ushort x=hl.x0; x<=hl.x1; ++x, ptr += output.channels) {
                    assert(ptr < res->data() + res->size());
                    if constexpr(output.channels == 0) {
                        if constexpr(output.channels == 1) {
                            *ptr = 255;
                        } else if constexpr(output.channels == 3) {
                            *(ptr + 0) = 255;
                            *(ptr + 1) = 255;
                            *(ptr + 2) = 255;
                        }
                    } else {
                        auto value = background.color<output>(x, hl.y);
                        if constexpr(output.channels == 1) {
                            *ptr = value;
                        } else if constexpr(output.channels == 3) {
                            *(ptr + 0) = value[0];
                            *(ptr + 1) = value[1];
                            *(ptr + 2) = value[2];
                        }
                    }
                }
            }
        });
        
        return {output, std::move(res)};
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
            if(channels() == 0) {
                if(!dont_cache || _recount_threshold == -1) {
                    _recount_threshold = threshold;
                    _recount = num_pixels();
                    return _recount;
                } else {
                    return num_pixels();
                }
            }
            
            if(_pixels == nullptr)
                throw U_EXCEPTION("Cannot threshold without pixel values.");
            
            float recount = 0;
#ifndef NDEBUG
            size_t local_recount = 0;
            auto local_ptr = _pixels->data();
#endif
            assert(_pixels && _pixels->data());
            auto ptr = _pixels->data();
            assert(_pixels->size() == num_pixels() * channels());
            
            InputInfo input{
                .channels = channels(),
                .encoding = encoding()
            };
            
            constexpr OutputInfo output{
                .channels = 1u,
                .encoding = meta_encoding_t::gray
            };
            
            call_image_mode_function<output>(input, KnownOutputType{}, [&]<InputInfo input, OutputInfo output, DifferenceMethod method>()
            {
                static_assert(is_in(input.channels, 0, 1, 3), "Only 0, 1 or 3 channels input is supported.");
                static_assert(is_in(output.channels, 1,3), "Only 1 or 3 channels output is supported.");
                
                for (auto &line : hor_lines()) {
                    if constexpr(input.channels == 0) {
                        recount += ptr_safe_t(line.x1) - ptr_safe_t(line.x0) + 1;
                        
                    } else {
                        const auto L = (ptr_safe_t(line.x1) - ptr_safe_t(line.x0) + 1) * input.channels;
                        recount += background.count_above_threshold<method, input>(line.x0, line.x1, line.y, std::span(ptr, ptr + L), threshold);
                        ptr += L;
#ifndef NDEBUG
                        for (auto x=line.x0; x<=line.x1; ++x, local_ptr += input.channels) {
                            auto value = diffable_pixel_value<input, output>(local_ptr);
                            if(background.is_different<output, method>(x, line.y, value, threshold)) {
                                local_recount++;
                            }
                        }
#endif
                    }
                }
            });
            
#ifndef NDEBUG
            if(recount != local_recount)
                FormatWarning("local_recount = ", local_recount, " vs. recount = ", recount);
            //assert(recount == local_recount);
#endif
            
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
    
    template<OutputInfo output, DifferenceMethod method>
    BlobPtr _threshold(Blob& blob, int32_t value, const Background& background) {
        if(blob.is_binary()) {
            return Blob::Make(blob);
        }
        
        if(blob.pixels() == nullptr)
            throw U_EXCEPTION("Cannot threshold without pixel values.");
        
        auto lines = std::make_unique<std::vector<HorizontalLine>>();
        lines->reserve(blob.hor_lines().size());
        
        auto ptr = blob.pixels()->data();
        HorizontalLine tmp;
        auto tmp_pixels = std::make_unique<std::vector<uchar>>();
        tmp_pixels->reserve(blob.pixels()->size());
        const auto input = blob.input_info();
        //OutputInfo output;
        //output = input;
        
        call_image_mode_function<output>(input, KnownOutputType{}, [&]<InputInfo input, OutputInfo, DifferenceMethod>()
        {
            static_assert(is_in(input.channels, 0, 1, 3), "Only 0, 1 or 3 channels input is supported.");
            static_assert(is_in(output.channels, 1,3), "Only 1 or 3 channels output is supported.");
            
            if constexpr(input.channels == 0) {
                *lines = blob.hor_lines();
            }
            
            for (auto &line : blob.hor_lines()) {
                if constexpr(input.channels == 0) {
                    tmp_pixels->insert(tmp_pixels->end(), (ptr_safe_t(line.x1) - ptr_safe_t(line.x0) + 1) * output.channels, 255);
                    
                } else {
                    tmp.x0 = line.x0;
                    tmp.y = line.y;
                    
                    for (auto x=line.x0; x<=line.x1; ++x, ptr += input.channels) {
                        assert(ptr < blob.pixels()->data() + blob.pixels()->size());
                        auto [pixel_value, grey_value] = dual_diffable_pixel_value<input, output>(ptr);
                        auto diff = background.diff<DIFFERENCE_OUTPUT_FORMAT, method>(x, line.y, grey_value);
                        if(background.is_value_different<output>(x, line.y, diff, value)) {
                            tmp.x1 = x;
                            push_pixel_value<output>(*tmp_pixels, pixel_value);
                            
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
            }
        });
        
        return Blob::Make(
            std::move(lines),
            std::move(tmp_pixels),
            blob.flags(),
            blob::Prediction{blob.prediction()}
        );
    }

    BlobPtr Blob::threshold(int32_t value, const Background& background) {
        OutputInfo output;
        output = input_info();
        return call_image_mode_function(input_info(), output, [&]<InputInfo input, OutputInfo output, DifferenceMethod method>() -> pv::BlobPtr {
            return _threshold<output, method>(*this, value, background);
        });
    }

    std::tuple<Vec2, Image::Ptr> Blob::gray_image(const cmn::Background* background, const Bounds& restricted, uchar padding) const {
        Bounds b(bounds().pos() - float(padding), bounds().size() + float(padding) * 2);
        if(background)
            b.restrict_to(background->bounds());
        else if(restricted.width > 0)
            b.restrict_to(restricted);
        else
            b.restrict_to(Bounds(0, 0, infinity<Float2_t>(), infinity<Float2_t>()));
        
        auto image = Image::Make(b.height, b.width, 1);
        
        if(!background)
            std::fill(image->data(), image->data() + image->size(), uchar(0));
        else {
            if(background->image().channels() == 1) {
                background->image().get()(b).copyTo(image->get());
            } else if(background->image().channels() == 3) {
                cv::cvtColor(background->image().get(), image->get(), cv::COLOR_BGR2GRAY);
            } else
                throw InvalidArgumentException("Background and blob are in incompatible combined formats: ", background->image(), " vs. ", *image);
        }
        
        auto _x = (coord_t)b.x;
        auto _y = (coord_t)b.y;
        assert(image->channels() == 1);
        
        auto work = [&]<InputInfo input, OutputInfo output, DifferenceMethod method>() {
            static_assert(is_in(input.channels, 0,1,3), "Only 0, 1 or 3 channels input is supported.");
            static_assert(is_in(output.channels, 1,3), "Only 1 or 3 channels output is supported.");
            
            const uchar* ptr;
            if constexpr(input.channels > 0) {
                assert(_pixels);
                ptr = _pixels->data();
            }
            
            for (auto &line : hor_lines()) {
                auto image_ptr = image->data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image->cols + ptr_safe_t(line.x0) - ptr_safe_t(_x)) * output.channels;
                if constexpr(input.channels == 0) {
                    const size_t N = line.length() * output.channels;
                    std::fill(image_ptr, image_ptr + N, 255);
                    image_ptr += N;
                    
                } else {
                    for (auto x=line.x0; x<=line.x1; ++x, ptr += input.channels, image_ptr += output.channels) {
                        assert(ptr < _pixels->data() + _pixels->size());
                        assert(image_ptr < image->data() + image->size());
                        *image_ptr = diffable_pixel_value<input, output>(ptr);
                    }
                }
            }
        };
        
        call_image_mode_function<OutputInfo{
            .channels = 1u,
            .encoding = meta_encoding_t::gray
        }>(input_info(), KnownOutputType{}, work);
        
        return {b.pos(), std::move(image)};
    }

    std::tuple<Vec2, Image::Ptr> Blob::color_image(const cmn::Background* background, const Bounds& restricted, uchar padding) const {
        Bounds b(bounds().pos() - float(padding), bounds().size() + float(padding) * 2);
        if(background)
            b.restrict_to(background->bounds());
        else if(restricted.width > 0)
            b.restrict_to(restricted);
        else
            b.restrict_to(Bounds(0, 0, infinity<Float2_t>(), infinity<Float2_t>()));
        
        const auto channels = this->channels();
        auto image = Image::Make(b.height, b.width, channels);
        
        if(!background)
            std::fill(image->data(), image->data() + image->size(), uchar(0));
        else {
            if(background->image().channels() == channels) {
                background->image().get()(b).copyTo(image->get());
            } else if(background->image().channels() == 1) {
                cv::cvtColor(background->image().get()(b), image->get(), cv::COLOR_GRAY2BGR);
            } else if(channels == 1) {
                cv::cvtColor(background->image().get()(b), image->get(), cv::COLOR_BGR2GRAY);
            } else
                throw InvalidArgumentException("Background and blob are in incompatible combined formats: ", background->image(), " vs. ", *image);
        }
        
        auto _x = (coord_t)b.x;
        auto _y = (coord_t)b.y;
        assert(image->channels() == channels);
        
        auto work = [&]<InputInfo input, OutputInfo output, DifferenceMethod method>() {
            static_assert(is_in(input.channels, 0, 1, 3), "Only 0, 1 or 3 channels input is supported.");
            static_assert(is_in(output.channels, 1,3), "Only 1 or 3 channels output is supported.");
            
            const uchar* ptr;
            if constexpr(input.channels > 0) {
                ptr = _pixels->data();
            }
            
#ifndef NDEBUG
            if(input != output)
                throw InvalidArgumentException("This method is designed to output exactly the same encoding + channels as the input.");
#endif
            
            for (auto &line : hor_lines()) {
                auto image_ptr = image->data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image->cols + ptr_safe_t(line.x0) - ptr_safe_t(_x)) * output.channels;
                if constexpr(input.channels == 0) {
                    const size_t N = line.length() * output.channels;
                    std::fill(image_ptr, image_ptr + N, 255);
                    image_ptr += N;
                    
                } else {
                    for (auto x=line.x0; x<=line.x1; ++x, ptr += input.channels, image_ptr += output.channels) {
                        assert(ptr < _pixels->data() + _pixels->size());
                        assert(image_ptr < image->data() + image->size());
                        for(uint8_t i = 0; i < input.channels; ++i)
                            *(image_ptr + i) = *(ptr + i);
                    }
                }
            }
        };
        
        OutputInfo output;
        output = input_info();
        
        call_image_mode_function(input_info(), output, work);
        return {b.pos(), std::move(image)};
    }
    
    Vec2 Blob::alpha_image(const cmn::Background& background, int32_t threshold, Image& image) const {
        Bounds b(bounds().pos()-Vec2(1), bounds().size()+Vec2(2));
        b.restrict_to(background.bounds());
        
        constexpr uint8_t out_channels = 4;
        if(image.rows != b.height
           || image.cols != b.width
           || image.channels() != out_channels)
        {
            image.create(b.height, b.width, out_channels);
        }
        
        std::fill(image.data(), image.data() + image.size(), uchar(0));
        
        auto _x = (coord_t)b.x;
        auto _y = (coord_t)b.y;
        
        float maximum = 0;
        
        auto work = [&]<InputInfo input, OutputInfo output, DifferenceMethod method>()
        {
            static_assert(is_in(input.channels, 0, 1, 3), "Only 0, 1 or 3 channels input is supported.");
            static_assert(is_in(output.channels, 1,3), "Only 1 or 3 channels output is supported.");
            
            const uchar* ptr;
            if constexpr(input.channels > 0) {
                ptr = _pixels->data();
            }
            
            for (auto &line : hor_lines()) {
                //auto image_ptr = image->data() + ((line.y - _y) * image->cols * out_channels + (line.x0 - _x) * out_channels);
                auto image_ptr = image.data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image.cols + (ptr_safe_t(line.x0) - ptr_safe_t(_x))) * out_channels;
                if constexpr(input.channels == 0) {
                    /// defaulting to 255 here for alpha as well,
                    /// this may be wrong
                    const size_t N = line.length() * out_channels;
                    std::fill(image_ptr, image_ptr + N, 255);
                    image_ptr += N;
                    
                } else {
                    for (auto x=line.x0; x<=line.x1; ++x, ptr += input.channels, image_ptr += out_channels) {
                        /// *out_channels* == 4, *output_channels* == 3
                        /// need to account for that here ^
                        assert(ptr < _pixels->data() + _pixels->size());
                        auto [pixel_value, grey_value] = dual_diffable_pixel_value<input, output>(ptr);
                        auto diff = background.diff<DIFFERENCE_OUTPUT_FORMAT, method>(x, line.y, grey_value);
                        if(background.is_value_different<DIFFERENCE_OUTPUT_FORMAT>(x, line.y, diff, threshold)) {
                            if(maximum < diff)
                                maximum = diff;
                            
                            write_pixel_value<output>(image_ptr, pixel_value);
                            *(image_ptr + output.channels) = diff;
                        }
                    }
                }
            }
        };
        
        constexpr OutputInfo output{
            .channels = out_channels-1,
            .encoding = meta_encoding_t::gray
        };
        
        call_image_mode_function<output>(input_info(), KnownOutputType{}, work);
        
        if(maximum > 0) {
            for (auto &line : hor_lines()) {
                auto image_ptr = image.data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image.cols + (ptr_safe_t(line.x0) - ptr_safe_t(_x))) * ptr_safe_t(out_channels);
                for (auto x=line.x0; x<=line.x1; ++x, image_ptr += out_channels) {
                    *(image_ptr + 3) = min(255, float(*(image_ptr + 3)) / (maximum * 0.6) * 255);
                }
            }
        }
        
        return b.pos();
    }

Vec2 Blob::rgba_image(const cmn::Background& background, int32_t threshold, Image& image, uint8_t padding) const {
    Bounds b(bounds().pos()-Vec2(padding), bounds().size()+Vec2(padding * 2));
    b.restrict_to(background.bounds());
    
    constexpr uint8_t out_channels = 4;
    if(image.rows != b.height
       || image.cols != b.width
       || image.channels() != out_channels)
    {
        image.create(b.height, b.width, out_channels);
    }
    
    std::fill(image.data(), image.data() + image.size(), uchar(0));
    
    auto _x = (coord_t)b.x;
    auto _y = (coord_t)b.y;
    
    auto work = [&]<InputInfo input, OutputInfo output, DifferenceMethod method>()
    {
        static_assert(is_in(input.channels, 0, 1, 3), "Only 0, 1 or 3 channels input is supported.");
        static_assert(is_in(output.channels, 1,3), "Only 1 or 3 channels output is supported.");
        
        const uchar* ptr;
        if constexpr(input.channels > 0) {
            ptr = _pixels->data();
        }
        
        for (auto &line : hor_lines()) {
            //auto image_ptr = image->data() + ((line.y - _y) * image->cols * out_channels + (line.x0 - _x) * out_channels);
            auto image_ptr = image.data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image.cols + (ptr_safe_t(line.x0) - ptr_safe_t(_x))) * out_channels;
            if constexpr(input.channels == 0) {
                /// defaulting to 255 here for alpha as well,
                /// this may be wrong
                const size_t N = line.length() * out_channels;
                std::fill(image_ptr, image_ptr + N, 255);
                image_ptr += N;
                
            } else {
                for (auto x=line.x0; x<=line.x1; ++x, ptr += input.channels, image_ptr += out_channels) {
                    /// *out_channels* == 4, *output_channels* == 3
                    /// need to account for that here ^
                    assert(ptr < _pixels->data() + _pixels->size());
                    auto [pixel_value, grey_value] = dual_diffable_pixel_value<input, output>(ptr);
                    if (threshold == 0
                        || background.is_different<DIFFERENCE_OUTPUT_FORMAT, method>(x, line.y, grey_value, threshold))
                    {
                        //if(background.is_value_different<output>(x, line.y, background.diff<output, method>(x, line.y, value), threshold)) {
                        write_pixel_value<output>(image_ptr, pixel_value);
                        *(image_ptr + output.channels) = 255;
                    }
                }
            }
        }
    };
    
    constexpr OutputInfo output{
        .channels = out_channels-1,
        .encoding = meta_encoding_t::gray
    };
    
    call_image_mode_function<output>(input_info(), KnownOutputType{}, work);
    return b.pos();
}

    std::tuple<Vec2, Image::Ptr> Blob::equalized_luminance_alpha_image(const cmn::Background& background, int32_t threshold, float minimum, float maximum, uint8_t padding, OutputInfo output) const {
        auto image = Image::Make();
        auto pos = equalized_luminance_alpha_image(background, threshold, minimum, maximum, *image, padding, output);
        return { pos, std::move(image) };
    }
    Vec2 Blob::equalized_luminance_alpha_image(const cmn::Background& background, int32_t threshold, float minimum, float maximum, Image& image, uint8_t padding, OutputInfo output) const 
    {
        if(not is_in(output.channels, 2, 4)) {
            throw InvalidArgumentException("This method produces an alpha channel, so only 2 or 4 channels are possible.");
        }
        
        Bounds b(bounds().pos()-Vec2(padding), bounds().size()+Vec2(padding));
        b.restrict_to(background.bounds());
        
        image.create(b.height, b.width, output.channels);
        std::fill(image.data(), image.data() + image.size(), uchar(0));
        
        auto _x = (coord_t)b.x;
        auto _y = (coord_t)b.y;
        minimum *= 0.5;
        
        float factor = 1;
        if(maximum > 0 && maximum != minimum)
            factor = 1.f / ((maximum - minimum) * 0.5) * 255;
        else
            minimum = 0;
        
        auto work = [&]<InputInfo input, OutputInfo output, DifferenceMethod method>()
        {
            static_assert(is_in(input.channels, 0, 1,3), "Only 1 or 3 channels input is supported.");
            static_assert(is_in(output.channels, 1,3), "Only 1 or 3 channels output is supported.");
            
            const uchar* ptr;
            if constexpr(input.channels > 0) {
                ptr = _pixels->data();
            }
            
            /// this is the real depth of the output image
            /// (which is output.channels + alpha channel)
            constexpr auto target_channels_alpha = output.channels + 1;
            
            for (auto &line : hor_lines()) {
                auto image_ptr = image.data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * ptr_safe_t(image.cols) + (ptr_safe_t(line.x0) - ptr_safe_t(_x))) * target_channels_alpha;
                
                if constexpr(input.channels == 0) {
                    /// defaulting to 255 here for alpha as well,
                    /// this may be wrong
                    const size_t N = line.length() * target_channels_alpha;
                    std::fill(image_ptr, image_ptr + N, 255);
                    image_ptr += N;
                    
                } else {
                    for (auto x=line.x0; x<=line.x1; ++x, ptr += input.channels, image_ptr += target_channels_alpha) {
                        assert(ptr < _pixels->data() + _pixels->size());
                        
                        auto [pixel_value, grey_value] = dual_diffable_pixel_value<input, output>(ptr);
                        auto diff = background.diff<DIFFERENCE_OUTPUT_FORMAT, method>(x, line.y, grey_value);
                        
                        if(threshold == 0
                           || background.is_value_different<DIFFERENCE_OUTPUT_FORMAT>(x, line.y, diff, threshold))
                        {
                            if constexpr(output.channels == 3) {
                                for(uint8_t p = 0; p < output.channels; ++p)
                                    *(image_ptr + p) = saturate((float(pixel_value[p]) - minimum) * factor);
                                
                            } else if constexpr(output.channels == 1) {
                                *image_ptr = saturate((float(pixel_value) - minimum) * factor);
                            }
                            
                            /// also set the alpha channel
                            *(image_ptr + output.channels) = saturate(int32_t(255 - SQR(1 - diff / 255.0) * 255.0));
                        }
                    }
                }
            }
        };
        
        call_image_mode_function(input_info(), OutputInfo{
            .channels = sign_cast<uint8_t>(output.channels - 1),
            .encoding = output.encoding
        }, work);
        
        return b.pos();
    }

    cmn::Vec2 Blob::luminance_alpha_image(const cmn::Background& background, int32_t threshold, Image& image, uint8_t padding, OutputInfo output) const {
        Bounds b(bounds().pos() - Vec2(padding), bounds().size() + Vec2(padding * 2));
        b.restrict_to(background.bounds());

#ifndef NDEBUG
        if(not is_in(output.channels, 4, 2)) {
            throw InvalidArgumentException("Cannot convert from ", output.channels, " channels to luminance_alpha. Need either luminance (2x8bit) or rgba as direct target.");
        }
#endif
        image.create(b.height, b.width, output.channels);
        std::fill(image.data(), image.data() + image.size(), uchar(0));

        auto _x = (coord_t)b.x;
        auto _y = (coord_t)b.y;
        
        auto work = [&]<InputInfo input, OutputInfo output, DifferenceMethod method>(){
            static_assert(is_in(input.channels, 0, 1, 3), "Must be 0, 1 or 3 channels.");
            static_assert(is_in(output.channels, 1, 3), "Must be 1 or 3 channels.");
            
            const uchar* ptr;
            if constexpr(input.channels > 0) {
                assert(_pixels);
                ptr = _pixels->data();
            }
            
            constexpr auto target_channels = output.channels + 1;
            
            for (auto& line : hor_lines()) {
                auto image_ptr = image.data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image.cols + (ptr_safe_t(line.x0) - ptr_safe_t(_x))) * target_channels;
                
                if constexpr(input.channels == 0) {
                    /// defaulting to 255 here for alpha as well,
                    /// this may be wrong
                    const size_t N = line.length() * target_channels;
                    std::fill(image_ptr, image_ptr + N, 255);
                    image_ptr += N;
                    
                } else {
                    for (auto x = line.x0; x <= line.x1; ++x, ptr += input.channels, image_ptr += target_channels) {
                        assert(ptr < _pixels->data() + _pixels->size());
                        
                        auto [pixel_value, grey_value] = dual_diffable_pixel_value<input, output>(ptr);
                        auto diff = background.diff<DIFFERENCE_OUTPUT_FORMAT, method>(x, line.y, grey_value);
                        
                        if (threshold == 0
                            || background.is_value_different<DIFFERENCE_OUTPUT_FORMAT>(x, line.y, diff, threshold))
                        {
                            write_pixel_value<output>(image_ptr, pixel_value);
                            *(image_ptr + output.channels) = saturate(int32_t(255 - SQR(1 - diff / 255.0) * 255.0) * 2);
                        }
                    }
                }
            }
        };
        
        call_image_mode_function(input_info(), OutputInfo{
             .channels = sign_cast<uint8_t>(output.channels - 1),
             .encoding = output.encoding
        }, work);
        
        return b.pos();
    }
    std::tuple<Vec2, Image::Ptr> Blob::luminance_alpha_image(const cmn::Background& background, int32_t threshold, uint8_t padding, OutputInfo output) const {
        auto image = Image::Make();
        Vec2 pos = luminance_alpha_image(background, threshold, *image, padding, output);
        return { pos, std::move(image) };
    }
    
    std::tuple<Vec2, Image::Ptr> Blob::difference_image(const cmn::Background& background, int32_t threshold) const {
        Bounds b(bounds().pos()-Vec2(1), bounds().size()+Vec2(2));
        b.restrict_to(background.bounds());
        
        auto _x = (coord_t)b.x;
        auto _y = (coord_t)b.y;
        auto ptr = _pixels ? _pixels->data() : nullptr;
        
        InputInfo input = input_info();
        OutputInfo output;
        output = input;
        
        /// minimal allowance of one output channel.
        /// we cannot output images with 0 channels, also even with 0 real-color channels
        /// the difference of binary will be 1 channel.
        if(output.encoding == meta_encoding_t::binary) {
            output.channels = 1;
            output.encoding = meta_encoding_t::gray;
        }
        
        auto image = Image::Make(b.height, b.width, output.channels);
        std::fill(image->data(), image->data() + image->size(), uchar(0));
        
        call_image_mode_function(input, output,
            [&]<InputInfo input, OutputInfo output, DifferenceMethod method>()
            {
                static_assert(is_in(input.channels, 0, 1, 3), "Must be 0, 1 or 3 channels.");
                static_assert(is_in(output.channels, 1, 3), "Must be 1 or 3 channels.");
                
                for (auto &line : hor_lines()) {
                    auto image_ptr = image->data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image->cols + ptr_safe_t(line.x0) - ptr_safe_t(_x)) * output.channels;
                    
                    if constexpr(input.channels == 0) {
                        const size_t N = line.length() * output.channels;
                        std::fill(image_ptr, image_ptr + N, 255);
                        image_ptr += N;
                        
                    } else {
                        for (auto x=line.x0; x<=line.x1; ++x, ptr += input.channels, image_ptr += output.channels) {
                            assert(not ptr || ptr < _pixels->data() + _pixels->size());
                            
                            auto [pixel_value, grey_value] = dual_diffable_pixel_value<input, output>(ptr);
                            auto diff = background.diff<DIFFERENCE_OUTPUT_FORMAT, method>(x, line.y, grey_value);
                            
                            if (threshold == 0
                                || background.is_value_different<DIFFERENCE_OUTPUT_FORMAT>(x, line.y, diff, threshold))
                            {
                                write_pixel_value<output>(image_ptr, pixel_value);
                            }
                        }
                    }
                }
            });
        
        return {b.pos(), std::move(image)};
    }
    
    void Blob::transfer_backgrounds(const cmn::Background &from, const cmn::Background &to, const Vec2& dest_offset) {
        InputInfo input = input_info();
        OutputInfo output;
        output = input;
        
        call_image_mode_function(input, output,
            [&]<InputInfo input, OutputInfo output, DifferenceMethod method>() {
            static_assert(is_in(input.channels, 0, 1, 3), "Must be 0, 1 or 3 channels.");
            static_assert(is_in(output.channels, 1, 3), "Must be 1 or 3 channels.");
            
            if constexpr(input.channels > 0) {
                assert(_pixels);
                auto ptr = _pixels->data();
                
                for (auto &line : hor_lines()) {
                    for (auto x=line.x0; x<=line.x1; ++x, ptr += input.channels) {
                        assert(ptr < _pixels->data() + _pixels->size());
                        auto pixel = diffable_pixel_value<input, output>(ptr);
                        pixel = saturate(pixel + to.color<output>(ptr_safe_t(x) + dest_offset.x, ptr_safe_t(line.y) + dest_offset.y) - from.color<output>(x, line.y), 0, 255);
                        
                        if constexpr(output.channels == 1) {
                            *(ptr + 0) = pixel;
                            
                        } else if constexpr(output.channels == 3) {
                            *(ptr + 0) = pixel[0];
                            *(ptr + 1) = pixel[1];
                            *(ptr + 2) = pixel[2];
                        }
                    }
                }
                
            } else {
                /// if we have 0 channels, we dont need to transfer anything...
            }
        });
    }
    
    decltype(Blob::_pixels) Blob::calculate_pixels(const Image::Ptr& image, const decltype(_hor_lines) &lines, const Vec2& offset) {
        auto pixels = std::make_unique<std::vector<uchar>>();
        for(auto &line : *lines) {
            auto start = image->ptr(uint(int64_t(line.y) + int64_t(offset.y)), uint(int64_t(line.x0) + int64_t(offset.x)));
            //auto start = image->data() + (ptr_safe_t(line.y) + ptr_safe_t(offset.y)) * image->cols + (ptr_safe_t(line.x0) + ptr_safe_t(offset.x));
            auto end = start + (ptr_safe_t(line.x1) - ptr_safe_t(line.x0) + 1) * image->channels();
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
        
        auto image = Image::Make(b.height, b.width, channels());
        std::fill(image->data(), image->data() + image->size(), uchar(0));
        
        auto _x = (coord_t)b.x;
        auto _y = (coord_t)b.y;
        
        auto work = [&]<InputInfo input, OutputInfo output, DifferenceMethod method>(){
            static_assert(is_in(input.channels, 0, 1, 3), "Must be 0, 1 or 3 channels.");
            static_assert(is_in(output.channels, 1, 3), "Must be 1 or 3 channels.");
            
            const uchar* ptr;
            if constexpr(input.channels > 0) {
                assert(_pixels);
                ptr = _pixels->data();
            }
            
            for (auto &line : hor_lines()) {
                auto image_ptr = image->data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image->cols + ptr_safe_t(line.x0) - ptr_safe_t(_x)) * output.channels;
                
                if constexpr(input.channels == 0) {
                    const size_t N = line.length() * output.channels;
                    std::fill(image_ptr, image_ptr + N, 255);
                    
                } else {
                    for (auto x=line.x0; x<=line.x1; ++x, ptr += input.channels, ++image_ptr) {
                        assert(ptr < _pixels->data() + _pixels->size());
                        
                        auto [pixel_value, grey_value] = dual_diffable_pixel_value<input, output>(ptr);
                        if (threshold == 0
                            || background.is_different<DIFFERENCE_OUTPUT_FORMAT, method>(x, line.y, grey_value, threshold))
                        {
                            //if(background.is_value_different<output>(x, line.y, background.diff<output, method>(x, line.y, value), threshold)) {
                            write_pixel_value<output>(image_ptr, pixel_value);
                        }
                    }
                }
            }
        };
        
        InputInfo input = input_info();
        OutputInfo output;
        output = input;
        
        call_image_mode_function(input, output, work);
        return {b.pos(), std::move(image)};
    }
    
    std::tuple<Vec2, Image::Ptr> Blob::binary_image(const cmn::Background& background, int32_t threshold) const {
        Bounds b(bounds().pos()-Vec2(1), bounds().size()+Vec2(2));
        b.restrict_to(background.bounds());
        
        auto image = Image::Make(b.height, b.width);
        std::fill(image->data(), image->data() + image->size(), uchar(0));
        
        auto _x = (coord_t)b.x;
        auto _y = (coord_t)b.y;
        
        auto work = [&]<InputInfo input, OutputInfo output, DifferenceMethod method>(){
            static_assert(is_in(input.channels, 0, 1, 3), "Must be 0, 1 or 3 channels.");
            
            const uchar* ptr;
            if constexpr(input.channels > 0) {
                if(_pixels == nullptr)
                    throw U_EXCEPTION("Cannot generate binary image without pixel values.");
                
                ptr = _pixels->data();
            }
            
            for (auto &line : hor_lines()) {
                auto image_ptr = image->data() + ((ptr_safe_t(line.y) - ptr_safe_t(_y)) * image->cols + ptr_safe_t(line.x0) - ptr_safe_t(_x)) * output.channels;
                
                if constexpr(input.channels == 0) {
                    const size_t N = line.length() * output.channels;
                    std::fill(image_ptr, image_ptr + N, 255);
                    
                } else {
                    for (auto x=line.x0; x<=line.x1; ++x, ptr += input.channels, image_ptr += output.channels) {
                        assert(ptr < _pixels->data() + _pixels->size());
                        
                        int32_t value = diffable_pixel_value<input, output>(ptr);
                        //value = background.diff<output, method>(x, line.y, value);
                        if(background.is_different<DIFFERENCE_OUTPUT_FORMAT, method>(x, line.y, value, threshold))
                            //if(background.is_value_different<output>(x, line.y, value, threshold))
                            *image_ptr = 255;
                    }
                }
            }
        };
        
        call_image_mode_function<OutputInfo{
            .channels = 1u,
            .encoding = meta_encoding_t::gray
        }>(input_info(), KnownOutputType{}, work);
        
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

void Blob::set_pixels(const cmn::blob::pixel_ptr_t::element_type& pixels)
{
    assert(pixels.size() == num_pixels() * channels());
    
    if(not _pixels)
    {
        _pixels = std::make_unique<pixel_ptr_t::element_type>();
    }
    
    (*_pixels) = pixels;
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

bool Blob::empty() const {
    return not _hor_lines || _hor_lines->empty();
}
    
    std::string Blob::toStr() const {
        return "pv::Blob<" + Meta::toStr(blob_id()) + " " + Meta::toStr(bounds().pos() + bounds().size() * 0.5) + " " + Meta::toStr(_pixels ? _pixels->size() * SQR(setting(cm_per_pixel)) : -1) + ">";
    }
}
