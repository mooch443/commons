
#pragma once

#include <sys/stat.h>
#include <misc/metastring.h>
#include <misc/useful_concepts.h>

namespace cmn::gui {
    class Color;
}

#include <misc/matharray.h>

namespace cmn {
    class Image;

    class CrashProgram {
    public:
        static std::thread::id crash_pid, main_pid;
        static bool do_crash;
        static void crash() { char *s = nullptr;
            *s = 0;}
    };
    
    /**
     * ======================
     * COMMON custom types
     * ======================
     */
    
    //! If this is set to true, horizontal lines will be ordered by CPULabeling according
    //  to their x/y coordinates. If not, then dilate for HorizontalLines is disabled, but
    //  CPULabeling runs a bit faster.
#define ORDERED_HORIZONTAL_LINES true
    
    /**
     * A structure that represents a horizontal line on an image.
     */
    using coord_t = uint16_t;
    using ptr_safe_t = uint64_t;
    struct HorizontalLine {
        coord_t x0, x1;
        coord_t y, padding;
        
        constexpr HorizontalLine() noexcept = default;
        constexpr HorizontalLine(coord_t y_, coord_t x0_, coord_t x1_) noexcept
            : x0(x0_), x1(x1_), y(y_), padding(0)
        {
            //assert(x0 <= x1);
        }
        
        constexpr bool inside(coord_t x_, coord_t y_) const noexcept {
            return y_ == y && x_ >= x0 && x_ <= x1;
        }
        
        constexpr bool overlap_x(const HorizontalLine& other) const noexcept {
            return other.x1 >= x0-1 && other.x0 <= x1+1;
        }
        
        constexpr bool overlap(const HorizontalLine& other) const noexcept {
            return other.y == y && overlap_x(other);
        }
        
        constexpr bool operator==(const HorizontalLine& other) const noexcept {
            return other.x0 == x0 && other.y == y && other.x1 == x1;
        }
        
        constexpr bool operator<(const HorizontalLine& other) const noexcept {
            //! Compares two HorizontalLines and sorts them by y and x0 coordinates
            //  (top-bottom, left-right)
            return y < other.y || (y == other.y && x0 < other.x0);
        }
        
        constexpr HorizontalLine merge(const HorizontalLine& other) const noexcept {
            //if(other.y != y)
            //    throw U_EXCEPTION("Cannot merge lines from y=",y," and ",other.y,"");
            //assert(overlap(other));
            return HorizontalLine(y, std::min(x0, other.x0), std::max(x1, other.x1));
        }
        
        std::string str() const {
            std::stringstream ss;
            ss << "HL<" << y << " " << x0 << "," << x1 << ">";
            return ss.str();
        }
        
        static void repair_lines_array(std::vector<HorizontalLine>&, std::vector<uchar>&);
        static void repair_lines_array(std::vector<HorizontalLine>&);
        
        std::string toStr() const;
        static std::string class_name() {
            return "HorizontalLine";
        }
    };
    
#if ORDERED_HORIZONTAL_LINES
    //! Returns an offset for y if it was needed to keep the values within 0<value<USHRT_MAX
    void dilate(std::vector<HorizontalLine>& array, int times=1, int max_cols = 0, int max_rows = 0);
#endif
    
    template <typename T>
    class NoInitializeAllocator : public std::allocator< T > {
    public:
        template <typename U>
        struct rebind {
            typedef NoInitializeAllocator<U> other;
        };
        
        //provide the required no-throw constructors / destructors:
        NoInitializeAllocator() throw() : std::allocator<T>() { };
        NoInitializeAllocator(const NoInitializeAllocator<T>& rhs) throw() : std::allocator<T>(rhs) { };
        ~NoInitializeAllocator() throw() { };
        
        //import the required typedefs:
        typedef T& reference;
        typedef const T& const_reference;
        typedef T* pointer;
        typedef const T* const_pointer;
        
        //redefine the construct function (hiding the base-class version):
        /*void construct( pointer p, const_reference cr) {
         Print("Construct called!");
         //else, do nothing.
         };*/
        
        template <class _Up, class... _Args>
        void
        construct(_Up*, _Args&&... )
        {
            // do nothing!
        }
    };
    template <class T, class U>
    bool operator==(const NoInitializeAllocator<T>&, const NoInitializeAllocator<U>&) { return true; }
    template <class T, class U>
    bool operator!=(const NoInitializeAllocator<T>&, const NoInitializeAllocator<U>&) { return false; }
    
    /**
     * Converts a lines array to a mask or greyscale.
     * Expects pixels to contain difference values instead of actual greyscale.
     * @deprecated backwards compatibility
     */
    /*std::pair<cv::Rect2i, size_t> imageFromLines_old(const std::vector<HorizontalLine>& lines,
     cv::Mat* output_mask,
     cv::Mat* output_greyscale = NULL,
     const std::vector<uchar>* pixels = NULL,
     const char threshold = 0,
     const cv::Mat* average = NULL);*/
    
    cv::Rect2i lines_dimensions(const std::vector<HorizontalLine>& lines);
    
    template <typename T = double>
    T normalize_angle(T angle) {
        while (angle < T(0.0)) angle += T(M_PI * 2);
        while (angle >= T(M_PI * 2)) angle -= T(M_PI * 2);
        return angle;
    }
    
    /**
     * The difference in angle to get from angle to vangle.
     */
    template <typename T = double, typename K = T>
    T angle_difference(T angle, K vangle) {
        T difference;
        
        if(std::abs(vangle-angle) < std::abs(T(M_PI*2)+angle-vangle) && std::abs(vangle-angle) < std::abs(T(M_PI*2)+vangle - angle))
            difference = vangle-angle;
        else {
            if(angle < vangle)
                difference = (T(M_PI*2)+angle) - vangle;
            else
                difference = (T(M_PI*2)+vangle) - angle;
        }
        
        return difference;
    }
    
    template <typename T = cv::Vec3b>
    T BGR2HSV(const T& rgb) {
        cv::Mat tmp(1, 1, CV_8UC3);
        tmp = rgb;
        cv::cvtColor(tmp, tmp, cv::COLOR_BGR2HSV);
        
        return tmp.at<cv::Vec3b>(0, 0);
    }
    
    template <typename T = cv::Vec3b>
    T HSV2RGB(const T& hsv) {
        cv::Mat tmp(1, 1, CV_8UC3);
        tmp.at<cv::Vec3b>(0, 0) = hsv;
        cv::cvtColor(tmp, tmp, cv::COLOR_HSV2BGR);
        
        return tmp.at<cv::Vec3b>(0, 0);
    }
    
    inline int ggt(int x, int y) { /* gibt ggt(x,y) zurueck, falls x oder y nicht 0 */
        int c;                /* und gibt 0 zurueck fuer x=y=0.   */
        if ( x < 0 ) x = -x;
        if ( y < 0 ) y = -y;
        while ( y != 0 ) {          /* solange y != 0 */
            c = x % y; x = y; y = c;  /* ersetze x durch y und
                                       y durch den Rest von x modulo y */
        }
        return x;
    }
    
    inline int isPowerOfTwo (unsigned int x)
    {
        return x && !(x & (x - 1));
        //return ((x != 0) && !(x & (x - 1)));
    }
    
    inline int gt(int x, int y) {
        for(int i=(x < y ? x : y); i>1; i--)
            if(x%i == 0)
                return i;
        return 1;
    }
    
    template<typename T>
    T lerp(T start, T end, ScalarType percent)
    {
        return (start + (end - start) * percent);
    }

    template<typename K, typename T = K>
        requires (not is_rgb_array<K>::value)
    constexpr inline T saturate(K val, T min = 0, T max = 255) {
        return std::clamp(T(val), min, max);
    }
    
    /**
     * ======================
     * INITIALIZER methods
     * ======================
     */
    
    template<int m, int n>
    inline cv::Matx<ScalarType, m, n> identity() {
        cv::Matx<ScalarType, m, n> mat;
        cv::setIdentity(mat);
        return mat;
    }
    
    template<int m=3, int n=1>
    inline cv::Matx<ScalarType, m, n> zeros() {
        return cv::Matx<ScalarType, m, n>::zeros();
    }
    
    template<int m=3, int n=1>
    inline cv::Matx<ScalarType, m, n> ones() {
        return cv::Matx<ScalarType, m, n>::ones();
    }
    
    template<int m=3, int n=1>
    inline cv::Matx<ScalarType, m, n> fill(ScalarType number) {
        cv::Matx<ScalarType, m, n> ret;
        for(int i=0; i<m; i++)
            for(int j=0; j<n; j++)
                ret(i, j) = number;
        
        return ret;
    }
    
    /**
     * ======================
     * PRINTING methods
     * ======================
     */
    
    template <typename _Tp = ScalarType, int m, int n>
    void print_mat(const char*name, const cv::Matx<_Tp, m, n>& mat) {
        printf("%s(%dx%d) = \n", name, m, n);
        for(int i=0; i<mat.rows; i++) {
            printf("[");
            for (int j=0; j<mat.cols; j++) {
                printf("\t%.3f", mat(i, j));
            }
            printf("\t]\n");
        }
        printf("\n");
    }
    
    inline std::string getImgType(int imgTypeInt);

    inline void print_mat(const char*name, const cv::Mat& mat) {
        auto type = getImgType(mat.type());
        printf("%s(%dx%d, %s) = \n", name, mat.rows, mat.cols, type.c_str());
        for(int i=0; i<mat.rows; i++) {
            printf("[");
            for (int j=0; j<mat.cols; j++) {
                switch (mat.type()) {
                    case CV_64FC1:
                        printf("\t%.6f", mat.at<double>(i, j));
                        break;
                    case CV_32FC1:
                        printf("\t%.6f", mat.at<float>(i, j));
                        break;
                    case CV_32FC3: {
                        auto v = mat.at<cv::Vec3f>(i, j);
                        printf("\t%.6f %.6f %.6f", v[0], v[1], v[2]);
                        break;
                    }
                    case CV_32SC2:
                        printf("\t(%d,%d)", mat.at<cv::Vec2i>(i, j)(0), mat.at<cv::Vec2i>(i, j)(1));
                        break;
                        
                    default:
                        type = getImgType(mat.type());
                        throw std::invalid_argument("Unknown data type: " + type);
                        break;
                }
            }
            printf("\t]\n");
        }
        printf("\n");
    }
    
    template <typename _Tp = ScalarType, int m, int n>
    void print_mat(const char*name, const std::initializer_list<cv::Matx<_Tp, m, n>>& l, int = m) {
        std::vector<cv::Matx<_Tp, m, n>> mats(l);
        
        printf("%s(%dx%d) = \n", name, m, n);
        for(int i=0; i<m; i++) {
            for(int k=0; k < mats.size(); k++) {
                auto& mat = mats[k];
                
                printf("[");
                for (int j=0; j<n; j++) {
                    printf("\t%.3f", mat(i, j));
                }
                printf("\t]");
            }
            printf("\n");
        }
        printf("\n");
    }
    
    template <typename _Tp = ScalarType, int m, int n>
    void print_mat(const char*name, const std::vector<cv::Matx<_Tp, m, n>>& mats) {
        printf("%s(%dx%d) = \n", name, m, n);
        for(int i=0; i<m; i++) {
            for(long k=0; k < (long)mats.size(); k++) {
                auto& mat = mats[k];
                
                printf("[");
                for (int j=0; j<n; j++) {
                    printf("\t%.3f", mat(i, j));
                }
                printf("\t]");
            }
            printf("\n");
        }
        printf("\n");
    }
    
    /**
     * ======================
     * IS_NAN methods
     * ======================
     */
    
    template <typename _Tp = ScalarType, int m, int n>
    bool is_nan(const cv::Matx<_Tp, m, n>& mat) {
        for(int i=0; i<m; i++) {
            for (int j=0; j<n; j++) {
                if(std::isnan(mat(i, j))) {
                    return true;
                }
            }
        }
        return false;
    }
    
    /**
     * ======================
     * MISC methods
     * ======================
     */
    
    constexpr inline bool between_equals(float x, float lower, float upper) {
        return x >= lower && x <= upper;
    }
    
    inline int is_big_endian(void)
    {
        union {
            uint32_t i;
            char c[4];
        } bint = {0x01020304};
        
        return bint.c[0] == 1;
    }
    
    inline std::ostream&
    duration_to_string(std::ostream& os, std::chrono::nanoseconds ns)
    {
        using namespace std;
        using namespace std::chrono;
        typedef duration<int, ratio<86400>> days;
        char fill = os.fill();
        os.fill('0');
        auto d = duration_cast<days>(ns);
        ns -= d;
        auto h = duration_cast<hours>(ns);
        ns -= h;
        auto m = duration_cast<minutes>(ns);
        ns -= m;
        auto s = duration_cast<seconds>(ns);
        os << setw(2) << d.count() << "d:"
        << setw(2) << h.count() << "h:"
        << setw(2) << m.count() << "m:"
        << setw(2) << s.count() << 's';
        os.fill(fill);
        return os;
    }
    
    // take number image type number (from cv::Mat.type()), get OpenCV's enum string.
    inline std::string getImgType(int imgTypeInt)
    {
        int numImgTypes = 35; // 7 base types, with five channel options each (none or C1, ..., C4)
        
        static const int enum_ints[] =       {CV_8U,  CV_8UC1,  CV_8UC2,  CV_8UC3,  CV_8UC4,
            CV_8S,  CV_8SC1,  CV_8SC2,  CV_8SC3,  CV_8SC4,
            CV_16U, CV_16UC1, CV_16UC2, CV_16UC3, CV_16UC4,
            CV_16S, CV_16SC1, CV_16SC2, CV_16SC3, CV_16SC4,
            CV_32S, CV_32SC1, CV_32SC2, CV_32SC3, CV_32SC4,
            CV_32F, CV_32FC1, CV_32FC2, CV_32FC3, CV_32FC4,
            CV_64F, CV_64FC1, CV_64FC2, CV_64FC3, CV_64FC4};
        
        static const std::string enum_strings[] = {"CV_8U",  "CV_8UC1",  "CV_8UC2",  "CV_8UC3",  "CV_8UC4",
            "CV_8S",  "CV_8SC1",  "CV_8SC2",  "CV_8SC3",  "CV_8SC4",
            "CV_16U", "CV_16UC1", "CV_16UC2", "CV_16UC3", "CV_16UC4",
            "CV_16S", "CV_16SC1", "CV_16SC2", "CV_16SC3", "CV_16SC4",
            "CV_32S", "CV_32SC1", "CV_32SC2", "CV_32SC3", "CV_32SC4",
            "CV_32F", "CV_32FC1", "CV_32FC2", "CV_32FC3", "CV_32FC4",
            "CV_64F", "CV_64FC1", "CV_64FC2", "CV_64FC3", "CV_64FC4"};
        
        for(int i=0; i<numImgTypes; i++)
        {
            if(imgTypeInt == enum_ints[i]) return enum_strings[i];
        }
        return "unknown image type";
    }
    
    inline bool file_exists(const std::string &path) {
#ifdef WIN32
		DWORD attr = GetFileAttributes(path.c_str());
		if (INVALID_FILE_ATTRIBUTES == attr /*|| (attr & FILE_ATTRIBUTE_DIRECTORY)*/)
		{
			return false;
		}
		return true;
#else
        struct stat buffer;
        return (stat (path.c_str(), &buffer) == 0);
#endif
    }
    
    template<typename T>
    inline void resize_image(T& mat, double factor, int flags = cv::INTER_NEAREST)
    {
        cv::resize(mat, mat, cv::Size(), factor, factor, flags);
    }
    
    template<typename T, typename K>
    inline void resize_image(const T& mat, K& output, double factor, int flags = cv::INTER_NEAREST)
    {
        cv::resize(mat, output, cv::Size(), factor, factor, flags);
    }

enum class ImageMode {
    GRAY,
    RGB,
    R3G3B2,
    RGBA
};

uint8_t required_channels(ImageMode mode);

template<ImageMode mode>
consteval uint8_t required_channels() noexcept {
    if constexpr(mode == ImageMode::GRAY
                 || mode == ImageMode::R3G3B2)
    {
        return 1;
        
    } else if constexpr(mode == ImageMode::RGB) {
        return 3;
        
    } else if constexpr(mode == ImageMode::RGBA) {
        return 4;
        
    } else {
        static_assert(is_in(mode,
                            ImageMode::GRAY,
                            ImageMode::R3G3B2,
                            ImageMode::RGB,
                            ImageMode::RGBA), "Unknown mode.");
    }
}

template<typename Vec>
constexpr uint8_t vec_to_r3g3b2(const Vec& bgr) {
    return (uint8_t(bgr[0] / 64) << 6)
         | (uint8_t(bgr[1] / 32) << 3)
         | (uint8_t(bgr[2] / 32) << 0);
}

template<uint8_t channels = 3>
constexpr auto r3g3b2_to_vec(const uint8_t& r3g3b2, const uint8_t alpha = 255) {
    if constexpr(channels == 3) {
        return RGBArray{
            static_cast<unsigned char>(((uint8_t(r3g3b2) >> 6) & 3) * 64),
            static_cast<unsigned char>(((uint8_t(r3g3b2) >> 3) & 7) * 32),
            static_cast<unsigned char>((uint8_t(r3g3b2) & 7) * 32)
        };
    } else if constexpr(channels == 4) {
        return MathArray<uchar, 4>{
            static_cast<unsigned char>(((uint8_t(r3g3b2) >> 6) & 3) * 64),
            static_cast<unsigned char>(((uint8_t(r3g3b2) >> 3) & 7) * 32),
            static_cast<unsigned char>((uint8_t(r3g3b2) & 7) * 32),
            alpha
        };
    }
}

template<uint8_t channels>
void convert_to_r3g3b2(const cv::Mat& input, cv::Mat& output) {
    if (output.rows != input.rows || output.cols != input.cols || output.type() != CV_8UC1) {
        output = cv::Mat::zeros(input.rows, input.cols, CV_8UC1);
    }
    assert(channels == input.channels());
    static_assert(channels == 3 || channels == 4, "Channels must be 3 or 4");

    using input_t = RGBArray;
    using output_t = uchar;

    auto process_row = [&](const cv::Range& range) {
        for (int y = range.start; y < range.end; y++) {
            auto input_row = input.ptr<input_t>(y);
            auto output_row = output.ptr<output_t>(y);
            for (int x = 0; x < input.cols; ++x) {
                //if constexpr(channels == 3)
                output_row[x] = vec_to_r3g3b2(input_row[x]);
                //output.at<uchar>(y, x) = vec_to_r3g3b2(input.at<cv::Vec3b>(y, x));
            //else if(channels == 4)
                //output.at<uchar>(y, x) = vec_to_r3g3b2(input.at<cv::Vec4b>(y, x));
            }
        }
    };

    cv::parallel_for_(cv::Range(0, input.rows), process_row);
}

template<uint8_t channels = 3, uint8_t input_channels = 1, bool generate_alpha = false>
void convert_from_r3g3b2(const cv::Mat& input, cv::Mat& output) {
    using input_t = MathArray<uchar, input_channels>;
    using output_t = MathArray<uchar, channels>;

    if (output.rows != input.rows
        || output.cols != input.cols
        || output.type() != CV_8UC(channels)) {
        output = cv::Mat(input.rows, input.cols, CV_8UC(channels));
    }

    auto process_row = [&](const cv::Range& range) {
        for (int y = range.start; y < range.end; ++y) {
            auto input_row = input.ptr<input_t>(y);
            auto output_row = output.ptr<output_t>(y);
            for (int x = 0; x < input.cols; ++x) {
                if constexpr (input_channels == 2)
                    output_row[x] = r3g3b2_to_vec<channels>(input_row[x][0], input_row[x][1]);
                else {
                    if constexpr(generate_alpha) {
                        output_row[x] = r3g3b2_to_vec<channels>(input_row[x][0], input_row[x][0]);
                    } else {
                        output_row[x] = r3g3b2_to_vec<channels>(input_row[x][0]);
                    }
                }
            }
        }
    };

    cv::parallel_for_(cv::Range(0, input.rows), process_row);
}

/*template<uint8_t channels = 3, uint8_t input_channels = 1>
void convert_from_r3g3b2(const cv::Mat& input, cv::Mat& output) {
    using input_t = cv::Vec<uchar, input_channels>;
    using output_t = cv::Vec<uchar, channels>;
    
    if(output.rows != input.rows
       || output.cols != input.cols
       || output.type() != CV_8UC(channels))
    {
        output = cv::Mat::zeros(input.rows, input.cols, CV_8UC(channels));
    }
    
    for(int y=0; y < input.rows; ++y) {
        for(int x=0; x < input.cols; ++x) {
            auto &i = input.at<input_t>(y, x);
            if constexpr(input_channels == 2)
                output.at<output_t>(y, x) = r3g3b2_to_vec<channels>(i[0], i[1]);
            else
                output.at<output_t>(y, x) = r3g3b2_to_vec<channels>(i[0]);
        }
    }
}*/
    
    // set all mat values at given channel to given value
    inline void setAlpha(cv::Mat &mat, unsigned char value, cv::Scalar only_this = cv::Scalar(0, 0, 0, -1))
    {
        // make sure have enough channels
        if (mat.channels() != 4)
            return;
        
        const int cols = mat.cols;
        const int step = mat.channels();
        const int rows = mat.rows;
        for (int y = 0; y < rows; y++) {
            // get pointer to the first byte to be changed in this row
            unsigned char *p_row = mat.ptr(y) + 3;
            unsigned char *row_end = p_row + cols*step;
            
            if(only_this(3) >= 0) {
                for (; p_row != row_end; p_row += step) {
                    if((*(p_row-3) == only_this(2)*255 && *(p_row-2) == only_this(1)*255) && *(p_row-1) == only_this(0)*255) {
                        *p_row = value;
                    }
                }
            } else {
                for (; p_row != row_end; p_row += step)
                    *p_row = value;
            }
        }
    }
    
    template< typename tPair >
    struct first_t {
        typename tPair::first_type operator()( const tPair& p ) const { return p.first; }
    };
    
    template< typename tMap >
    first_t< typename tMap::value_type > first( const tMap& ) { return first_t< typename tMap::value_type >(); }
    
    template<typename Map, typename KeyType = typename Map::key_type>
    inline std::set<KeyType> extract_keys(const Map& m) {
        std::set<KeyType> v;
        std::transform( m.begin(), m.end(), std::inserter( v, v.end() ), first(m) );
        return v;
    }
    
    template< typename tPair >
    struct second_t {
        typename tPair::second_type operator()( const tPair& p ) const { return p.second; }
    };
    
    template< typename tMap >
    second_t< typename tMap::value_type > second( const tMap& ) { return second_t< typename tMap::value_type >(); }
    
    template<typename KeyType, typename ValueType>
    inline void extract_values(const std::map<KeyType, ValueType>& m, std::vector<ValueType>& v) {
        std::transform( m.begin(), m.end(), std::back_inserter( v ), second(m) );
    }
    
    template<typename KeyType, typename ValueType>
    inline std::vector<ValueType> extract_values(const std::map<KeyType, ValueType>& m) {
        std::vector<ValueType> v;
        extract_values(m, v);
        return v;
    }
    
    //! This interface adds a "minimize_memory()" function that should
    //  make the class more compressed and reduce memory usage to a minimum.
    class Minimizable {
    public:
        virtual void minimize_memory() = 0;
        virtual ~Minimizable() {}
    };
    
    //! Escapes html reserved characters in a string
    inline std::string escape_html(const std::string_view& data) {
        std::string buffer;
        buffer.reserve(size_t(data.size()*1.1f));
        for(size_t pos = 0; pos != data.size(); ++pos) {
            switch(data[pos]) {
                case '&':  buffer.append("&amp;");       break;
                case '\"': buffer.append("&quot;");      break;
                case '\'': buffer.append("&apos;");      break;
                case '<':  buffer.append("&lt;");        break;
                case '>':  buffer.append("&gt;");        break;
                default:   buffer.append(&data[pos], 1); break;
            }
        }
        return buffer;
    }
        
    /**
     * T is the destination type, V is a primitive Type that can be copied easily.
     * V has to have a method convert(T&) for assignment.
     * Objects of type T are constructed from elements of type V in compare.
     * Excess objects within vector are deleted.
     * @requires Source type needs a method convert(Target*),
     *           Target needs empty constructor
     */
    template<typename T, typename K = T, typename V, class... D, template <typename, class...> class PtrType, class... DV, template <typename, class...> class PtrTypeV>
    inline void update_vector_elements(std::vector<PtrType<T, D...>>& vector,
                                       const std::vector<PtrTypeV<V, DV...>>& compare,
                                       const std::function<void(const PtrType<T, D...>&, const PtrTypeV<V, DV...>&)>& prepare = nullptr) {
        // Resize vector to match compare, default-constructing new elements as needed
        if (vector.size() > compare.size()) {
            vector.resize(compare.size());
        }

        // Prepare and convert existing elements
        for (size_t i = 0; i < vector.size(); ++i) {
            if (prepare) {
                prepare(vector[i], compare[i]);
            }
            compare[i]->convert(vector[i]);
        }

        // Add and prepare missing elements
        for (size_t i = vector.size(); i < compare.size(); ++i) {
            PtrType<T, D...> ptr = std::make_unique<K>();
            vector.emplace_back(std::move(ptr));

            if (prepare) {
                prepare(vector.back(), compare[i]);
            }
            compare[i]->convert(vector.back());
        }
    }
    
    inline uint8_t hardware_concurrency() noexcept {
#if  defined(__EMSCRIPTEN__)
        return 1u;
#else
#if TRACKER_GLOBAL_THREADS
        return TRACKER_GLOBAL_THREADS;
#else
        static const auto c = (uint8_t)saturate(std::thread::hardware_concurrency(), 1u, 255u);
        return c;
#endif
#endif
    }

    //! Defined in GlobalSettings.h
    struct SettingsMaps;
    namespace sprite {
        class Map;

        template<typename T>
        concept _has_str_method = requires(T t) {
            { t.str() } -> std::convertible_to<std::string>;
        };

        struct MapSource {
            std::string name;

            MapSource(_has_str_method auto&& s) : name(s.str()) { }

            MapSource(utils::StringLike auto&& n) : name(n) { }

            static constexpr std::string_view CMD{"/CMD"}, defaults{"/DEFAULTS"};
        };
        //! Parses a JSON-like object from string {"key": "value", "key2": 123, "key3": ["test","strings"]}
        Map parse_values(MapSource, std::string str, const sprite::Map* additional = nullptr);
        std::set<std::string> parse_values(MapSource, Map&, std::string, const sprite::Map* additional = nullptr, const std::vector<std::string>& exclude = {}, const std::map<std::string, std::string>& deprecations = {});
    }
    
    void set_thread_name(const std::string& name);
    std::string get_thread_name();
    
    class Viridis {
    public:
        using value_t = std::tuple<double, double, double>;
    private:
        static const std::array<value_t, 256> data_bgr;
    public:
        static gui::Color value(double percent);
    };


    template<typename VT>
    auto cvt2json(const VT & v) -> nlohmann::json {
        if constexpr (is_numeric<VT>)
            return nlohmann::json(v);
        else if constexpr (_clean_same<VT, bool>)
            return nlohmann::json((bool)v);
        else if constexpr (is_map<VT>::value) {
            auto a = nlohmann::json::object();
            for (auto& [key, i] : v) {
                if constexpr(std::is_same_v<decltype(key), std::string>)
                    a[key] = cvt2json(i);
                else if constexpr(std::is_convertible_v<decltype(key), std::string>)
                    a[std::string(key)] = cvt2json(i);
                else
                    a[cmn::Meta::toStr(key)] = cvt2json(i);
            }
            return a;
        }
        else if constexpr (is_container<VT>::value || is_set<VT>::value) {
            auto a = nlohmann::json::array();
            for (const auto& i : v) {
                a.push_back(cvt2json(i));
            }
            return a;
        }
        else if constexpr (std::is_same_v<VT, std::string>) {
            std::string str = v;
            return nlohmann::json(v.c_str());
        }
        else if constexpr (std::is_convertible_v<VT, std::string>) {
            std::string str = v;
            return nlohmann::json(v.c_str());
        }
        else if constexpr (is_pair<VT>::value) {
            auto a = nlohmann::json::array();
            a.push_back(cvt2json(v.first));
            a.push_back(cvt2json(v.second));
            return a;
        }
        else if constexpr (is_tuple_v<VT>) {
            auto a = nlohmann::json::array();
            std::apply([&a](auto&&... args) { ((a.push_back(cvt2json(args))), ...); }, v);
            return a;
        }
        else if constexpr(has_to_json_method<VT>) {
            return v.to_json();
        }
        else if constexpr(are_the_same<cv::Mat, VT>) {
            return cmn::Meta::toStr(v);
        }
        //auto str = Meta::toStr(v);
        //return nlohmann::json(str.c_str());
        
        else {
            static_assert(std::same_as<const VT, std::remove_cvref_t<VT>*>, "Cannot convert object to json.");
        }
            //throw U_EXCEPTION("Cannot parse JSON for: ", v, " of type ", type_name<VT>());
    }

template<typename T>
class GuardedProperty {
private:
    mutable std::mutex _mutex;
    T _property;
    bool _property_changed;

public:
    GuardedProperty() : _property_changed(false) {}

    explicit GuardedProperty(const T& initial_value)
        : _property(initial_value), _property_changed(false) {}

    bool set(const T& new_value) {
        std::lock_guard<std::mutex> lock(_mutex);
        if constexpr(requires (T t) {
            { t != t } -> std::convertible_to<bool>;
        }) {
            if (_property != new_value)
            {
                _property = new_value;
                _property_changed = true;
                return true;
            }
        } else {
            _property = new_value;
            _property_changed = true;
            return true;
        }
        return false;
    }

    std::optional<T> getIfChanged() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_property_changed) {
            _property_changed = false;
            return _property;
        }
        return std::nullopt;
    }

    T get() {
        std::lock_guard<std::mutex> lock(_mutex);
        _property_changed = false;
        return _property;
    }

    bool hasChanged() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _property_changed;
    }
};

class ExtendableVector : public std::vector<std::string> {
public:
    // Inherit constructors
    using std::vector<std::string>::vector;

    // Overload the + operator
    ExtendableVector operator+(const ExtendableVector& other) const {
        ExtendableVector result(*this);  // Start with a copy of the current object
        result.insert(result.end(), other.begin(), other.end());
        result.clean();
        return result;
    }
    
    // Templated + operator to handle StringLike types
    template<typename Container>
        requires utils::StringLike<typename Container::value_type>
    ExtendableVector operator+(const Container& other) const {
        ExtendableVector result(*this);
        for (const auto& element : other) {
            if constexpr (std::is_array_v<std::remove_cvref_t<typename Container::value_type>>) {
                // Handle C-style strings (char arrays) specifically
                result.emplace_back(element, std::size(element) - 1);  // -1 to ignore null terminator
            } else {
                // Handle other string-like types
                result.emplace_back(element);  // Convert to string and add
            }
        }
        result.clean();
        return result;
    }
    
    // Templated += operator to handle StringLike types
    template<typename Container>
        requires utils::StringLike<typename Container::value_type>
    ExtendableVector& operator+=(const Container& other) {
        for (const auto& element : other) {
            if constexpr (std::is_array_v<std::remove_cvref_t<typename Container::value_type>>) {
                // Handle C-style strings (char arrays) specifically
                emplace_back(element, std::size(element) - 1);  // -1 to ignore null terminator
            } else {
                // Handle other string-like types
                emplace_back(element);  // Convert to string and add
            }
        }
        clean();
        return *this;
    }
    
    void clean() {
        std::set<std::string> unique;
        for(auto it = begin(); it != end(); ) {
            if(not unique.contains(*it)) {
                unique.insert(*it);
                ++it;
            } else {
                it = erase(it);
            }
        }
    }
    
    // Method to get a const reference to the vector
    const std::vector<std::string>& toVector() const {
        return *this;
    }
    
    std::string toStr() const {
        auto set = std::set<std::string>{begin(), end()};
        return Meta::toStr(set);
    }
};


}
