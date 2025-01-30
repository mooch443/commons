#include "VideoSource.h"
#include "Video.h"
#include <file/Path.h>
#include <misc/GlobalSettings.h>
#include <misc/ThreadPool.h>
#include <misc/Image.h>
#include <video/AveragingAccumulator.h>
#include <misc/ranges.h>
#include <processing/Background.h>

namespace cmn {

std::string load_string(const file::Path& npz, const std::string fname) {
    libzip::archive zip(npz.str(), ZIP_RDONLY);
    cnpy::npz_t arrays;

    for (auto stat : zip) {
      if (!(stat.valid & (ZIP_STAT_NAME | ZIP_STAT_SIZE))) {
        // Skip files without name or size
        continue;
      }
      std::string name(stat.name);
        
      auto file = zip.open(name);

      // erase the lagging .npy
      std::string varname = name.substr(0, name.size() - 4);
        
    if(varname == fname) {
        std::string buffer = file.read(stat.size);

        std::vector<size_t> shape;
        size_t word_size;
        bool fortran_order;
        cnpy::parse_npy_header(buffer, word_size,
                               shape, fortran_order);

        shape = {word_size};
        word_size = 4;
        
        cnpy::NpyArray format(shape, word_size, fortran_order);

        uint64_t offset = stat.size - format.num_bytes();
        memcpy(format.data<unsigned char>(), buffer.data() + offset,
               format.num_bytes());
        
        std::stringstream ss;
        for(auto ptr = format.data<uchar>(), end = format.data<uchar>() + format.num_bytes(); ptr != end; ptr += format.word_size) {
            ss << (char)*ptr;
        }
        
        return ss.str();
    }
    }
    
    return std::string();
}

std::vector<std::pair<std::string, VideoSource::File::Type>> VideoSource::File::_extensions = {
    { "mov", VIDEO },
    { "mp4", VIDEO },
    { "h264", VIDEO },
    { "h265", VIDEO },
    { "hevc", VIDEO },
    { "mpg", VIDEO },
    { "mkv", VIDEO },
    { "mxf", VIDEO },
    { "avi", VIDEO },
    { "gif", VIDEO },
    { "mts", VIDEO },
    { "tiff", IMAGE },
    { "tif", IMAGE },
    { "jpg", IMAGE },
	{ "png", IMAGE },
    { "bmp", IMAGE }
};

namespace video_cache {

struct CVideo {
    VideoSource::File::Type type{VideoSource::File::Type::UNKNOWN};
    file::Path path;
    Size2 resolution;
    Frame_t N_frames;
    uint32_t frame_rate{0};
    bool has_timestamps{false};
};

std::mutex mutex;
std::unordered_map<std::string, std::vector<CVideo>> storage;

std::vector<CVideo> _create_cache(const file::PathArray& source) {
    std::smatch m;
    std::regex rplaceholder ("%[0-9]+(\\.[0-9]+(.[1-9][0-9]*)?)?d");
    std::regex rext (".*(\\..+)$");
    
    std::string prefix, suffix, extension;
    auto str = source.source();
    if(std::regex_search(str,m,rext)) {
        auto x = m[1];
        extension = x.str().substr(1);
        prefix = str.substr(0u, (uint64_t)m.position(1));
        
        //Print("Extension ",extension," basename ",prefix);
        
    } else {
        throw std::runtime_error("Video extension not found in "+source.toStr()+". Please make sure this file is in a compatible format and you are not trying to 'convert' a .pv file when you should be trying to track it instead (-task track instead of -task convert).");
    }
    
    if(prefix.empty()) {
        prefix = "array";
    }
    
    //Print("Searching for ",prefix,".",extension);
    auto base = file::find_basename(source); //prefix;
    //Print("Found _base = ", _base, " for ", source);
    //_base = source.source();
    std::vector<CVideo> video_info;
    std::optional<Size2> first_resolution;
    
    for(auto &path : source) {
        auto extension = std::string(path.extension());
        auto basename = path.remove_extension().str();
        auto f = VideoSource::File::open(video_info.size(), basename, extension);
        if(!f)
            throw U_EXCEPTION("Cannot open file ",path.str(),".");
        
        Size2 resolution;
        if(f->type() == VideoSource::File::Type::IMAGE) {
            if(not first_resolution)
                first_resolution = f->resolution();
            resolution = *first_resolution;
        }
        
        video_info.push_back(CVideo{
            .type = f->type(),
            .path = f->filename(),
            .resolution = resolution,
            .N_frames = f->length(),
            .frame_rate = static_cast<uint32_t>(f->framerate()),
            .has_timestamps = f->has_timestamps()
        });
    }
    
    return video_info;
}

std::vector<CVideo> load_cache(const file::PathArray& source) {
    std::unique_lock guard(mutex);
    auto it = storage.find(source.source());
    if(it == storage.end()) {
        try {
            auto info = _create_cache(source);
            storage[source.source()] = info;
            return info;
            
        } catch(const std::exception& ex) {
#ifndef NDEBUG
            FormatExcept("Cannot create cache for ", utils::ShortenText(source.toStr(), 1000),": ", ex.what());
#endif
            throw;
        }
    }
    return it->second;
}

}

std::string VideoSource::File::complete_name(const std::string &basename, const std::string &ext) {
    return basename + "." + ext;
}

VideoSource::File * VideoSource::File::open(size_t index, const std::string& basename, const std::string& ext, bool no_check) {
    if(no_check) {
        return new File(index, basename, ext);
    }
    
    if (file_exists(complete_name(basename, ext))) {
        return new File(index, basename, ext);
    } //else {
        //throw U_EXCEPTION("Cannot open file '",basename,".",ext,"'.");
    //}
    
    return NULL;
}

VideoSource::File::File(File&& other)
: _index(other._index),
  _filename(other._filename),
  _length(other._length),
  _video(other._video ? other._video : nullptr),
  _type(other._type),
  _format(std::move(other._format)),
  _timestamps(std::move(other._timestamps)),
  _size(other._size)
{
    //if(other._video)
    //    delete other._video;
    other._video = nullptr;
}

VideoSource::File::File(size_t index, file::Path path, Frame_t length, Size2 size, uint32_t frame_rate, Type type)
    : _index(index), _filename(path), _length(length), _size(size), _video(), _type(type), _frame_rate(frame_rate)
{
    if(type == Type::VIDEO)
        _video = new FfmpegVideoCapture{""};
}

VideoSource::File::File(size_t index, const std::string& basename, const std::string& extension) : _index(index), _video(NULL), _size(0, 0) {
    _filename = complete_name(basename, extension);
    if(not file::Path(_filename).is_absolute())
        _filename = file::Path(_filename).absolute().str();

    // check the extension(-type)
    _type = UNKNOWN;
    for (auto ext : _extensions) {
        if (ext.first == utils::lowercase(extension)) {
            _type = ext.second;
            break;
        }
    }
    
    if (_type == UNKNOWN)
        throw U_EXCEPTION("Unknown extension ",extension," for file ",_filename,".");
    
    switch (_type) {
        case VIDEO: {
            _video = new FfmpegVideoCapture("");
            auto npz = file::Path(_filename).replace_extension("npz");
            if(npz.exists()) {
                try {
                    static bool message = false;
                    if(!message) {
                        Print("Found timestamps for file ", npz.str(),".");
                        message = true;
                    }
                    
                    _format = load_string(npz, "format");
                    _timestamps = cnpy::npz_load(npz.str(), "frame_time").as_vec<double>();
                    auto res = cnpy::npz_load(npz.str(), "imgshape").as_vec<int64_t>();
                    _size = cv::Size( (int)res[1], (int)res[0] );
                    _length = Frame_t(_timestamps.size());
                } catch(...) {
                    FormatExcept("Failed opening NPZ archive ",npz.str()," with (presumably) timestamps in them for video ",_filename,". Proceeding without.");
                    
                    if(!_video->open(_filename))
                        throw U_EXCEPTION("Opening Video ",_filename," failed. Please check whether it is accessible and intact.");
                    _length = Frame_t(_video->length());
                    _video->close();
                }
                
            } else {
                if(!_video->open(_filename))
                    throw U_EXCEPTION("Opening Video ",_filename," failed. Please check whether it is accessible and intact.");
                try {
                    _length = Frame_t(_video->length());
                } catch(const std::exception& e) {
                    FormatExcept("Exception while retrieving length of video ", _filename,": ", e.what());
                }
                _video->close();
            }
            break;
        }
            
        case IMAGE:
            _length = 1_f;
            break;
            
        default:
            break;
    }
}

bool VideoSource::File::frame(cmn::ImageMode color, Frame_t frameIndex, Image& output, cmn::source_location) const
{
    switch (_type) {
        case VIDEO: {
            if (!_video->is_open()) {
                _video->open(_filename);
                //_video->set_colored(color);
            } //else if(_video->colored() != color)
              //_video->set_colored(color);
            
            if (!_video->is_open())
                throw U_EXCEPTION("Video ",_filename," cannot be opened.");
            
            if(_video->read(frameIndex.get(), output)) {
                output.set_index(frameIndex.get());
                return true;
            }
            return false;
        }
            
        case IMAGE: {
            auto op = output.get();
            load_image_to_format(color, _filename, op);
            output.set_index(frameIndex.get());
            return true;
        }
            
        default:
            throw U_EXCEPTION("Grabbing frame ",frameIndex," from '",_filename,"' failed because the type was unknown.");
    }
}

bool VideoSource::File::frame(ImageMode color, Frame_t frameIndex, cv::Mat& output, cmn::source_location) const {
    switch (_type) {
    case VIDEO: {
        if (!_video->is_open()) {
            _video->open(_filename);
            //_video->set_colored(color);
        } //else if(_video->colored() != color)
          //_video->set_colored(color);

        if (!_video->is_open())
            throw U_EXCEPTION("Video ", _filename, " cannot be opened.");

        return _video->read(frameIndex.get(), output);
    }

    case IMAGE:
        load_image_to_format(color, _filename, output);
        return true;

    default:
        throw U_EXCEPTION("Grabbing frame ", frameIndex, " from '", _filename, "' failed because the type was unknown.");
    }
}

void VideoSource::File::frame(ImageMode color, Frame_t frameIndex, gpuMat& output, bool, cmn::source_location) const {
    switch (_type) {
        case VIDEO: {
            if (!_video->is_open()) {
                _video->open(_filename);
                //_video->set_colored(color);
            } //else if(_video->colored() != color)
              //_video->set_colored(color);
            
            if (!_video->is_open())
                throw U_EXCEPTION("Video ",_filename," cannot be opened.");
            
            _video->read(frameIndex.get(), output);
            
            assert(output.cols == _video->dimensions().width
                   && output.rows == _video->dimensions().height);
            
            break;
        }
            
        case IMAGE:
            load_image_to_format(color, _filename, output);
            break;
            
        default:
            throw U_EXCEPTION("Grabbing frame ",frameIndex," from '",_filename,"' failed because the type was unknown.");
    }
}

bool VideoSource::File::has_timestamps() const {
    switch (_type) {
        case VIDEO:
            /*if (!_video->isOpened())
                _video->open(_filename);
            
            output = _video->frame(frameIndex);*/
            return !_timestamps.empty();
            
        case IMAGE:
            return false;
            
        default:
            break;
    }
    
    throw U_EXCEPTION("Retrieving timestamp for ",_filename," failed because the type was unknown.");
}

short VideoSource::File::framerate() {
    if(type() != VIDEO)
        return -1;
    
    if(_frame_rate)
        return _frame_rate.value();
    
    if(has_timestamps()) {
        if(_timestamps.size() > 1) {
            auto prev = _timestamps[0];
            double average = 0;
            for (uint64_t i=1; i<_timestamps.size(); i++) {
                average += _timestamps[i] - prev;
                prev = _timestamps[i];
            }
            average = average / double(_timestamps.size()-1);
            _frame_rate = narrow_cast<uint32_t>(round(1. / average));
            return _frame_rate.value();
        } else
            return -1;
        
    } else {
        bool was_open = _video->is_open();
        if(not was_open)
            _video->open(_filename);
        auto fps = _video->frame_rate();
        if(not was_open)
            _video->close();
        _frame_rate = narrow_cast<uint32_t>(fps);
        return _frame_rate.value();
    }
}

timestamp_t VideoSource::File::timestamp(Frame_t frameIndex, cmn::source_location loc) const {
    if(_type != VIDEO)
        throw _U_EXCEPTION(loc, "Cannot retrieve timestamps from anything else other than videos.");
    
    if(!has_timestamps())
        throw _U_EXCEPTION(loc, "No timestamps available for ",_filename,".");
    
    if(not frameIndex.valid())
        throw _U_EXCEPTION(loc, "Frame index in timestamp() is invalid.");
    
    auto times = _timestamps[frameIndex.get()];
    auto point = std::chrono::duration<double>(times);
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(point);
    //uint64_t seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    //uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    //auto ns = timestamp_t(std::chrono::duration_cast<std::chrono::microseconds>(duration));
    return timestamp_t(duration);
}

const cv::Size& VideoSource::File::resolution() {
    if(_size.width == 0 && _size.height == 0) {
        switch(_type) {
            case VIDEO: {
                bool was_open = _video->is_open();
                if(not was_open)
                    _video->open(_filename);
                _size = _video->dimensions();
                if(not was_open)
                    _video->close();
                break;
            }
                
            case IMAGE: {
                auto output = cv::imread(_filename, cv::IMREAD_GRAYSCALE);
                _size = {output.cols, output.rows};
                break;
            }
            default:
                throw U_EXCEPTION("Retrieving size from ",_filename," failed because the type was unknown.");
        }
    }
    
    return _size;
}

void VideoSource::File::close() const {
    if (_type == VIDEO) {
        _video->close();
    }
}

VideoSource::File::~File() {
    if (_video) {
        delete _video;
    }
}

VideoSource::~VideoSource() {
    for (auto info : _files_in_seq) {
        delete info;
    }
}

std::string VideoSource::toStr() const {
    return "VideoSource<'" + utils::ShortenText(_source, 1000) + "' " + Meta::toStr(length()) + " frames>";
}

VideoSource::VideoSource(VideoSource&& other)
    :   _source(other._source),
        _base(other._base),
        _last_file(nullptr),
        _size(other._size),
        _length(other._length),
        _average(std::move(other._average)),
        _mask(std::move(other._mask)),
        _has_timestamps(other._has_timestamps),
        _framerate(other._framerate),
        _colors(other._colors)
{
    for(auto& f : other._files_in_seq) {
        _files_in_seq.push_back(new File(std::move(*f)));
        if(other._last_file == f)
            _last_file = _files_in_seq.back();
    }
    
}

VideoSource::VideoSource(const file::PathArray& source)
{
    auto cache = video_cache::load_cache(source);
    _base = file::find_basename(source);
    
    if(cache.empty()) {
        throw U_EXCEPTION("Cannot load video sequence ",source," (it is empty).");
    }
    
    size_t index = 0;
    for(auto &c : cache) {
        auto file = new File(index++, c.path, c.N_frames, c.resolution, c.frame_rate, c.type);
        _files_in_seq.emplace_back(file);
        _length += c.N_frames;
    }
    
    _size = _files_in_seq.front()->resolution();
    _has_timestamps = _files_in_seq.front()->has_timestamps();
    
    if(type() == File::VIDEO) {
        _framerate = _files_in_seq.front()->framerate();
    } else {
        //! TODO: Frame rate not being set for image sequences...
        //! needs check!
        FormatWarning("No frame rate can be set automatically for a sequence of images. Defaulting to ", framerate(),".");
    }
    
    for(auto f : _files_in_seq) {
        if(f->resolution() != _size)
            throw U_EXCEPTION(f->filename(), " has a different resolution ", f->resolution(), " than the first, which is ", _size,". Not supporting heterogenous resolutions in VideoSources.");
    }
}

void VideoSource::open(const std::string& prefix, const std::string& suffix, const std::string& extension, int seq_start, int seq_end, int padding)
{
    if (   seq_start == VIDEO_SEQUENCE_INVALID_VALUE
        || seq_end   == VIDEO_SEQUENCE_INVALID_VALUE)
    {
        File *f = File::open(0, prefix + suffix, extension);

        if(f && f->type() != File::VIDEO)
            FormatWarning("Just loading one image because seq_end/seq_start were not specified.");

        if(f) {
            _files_in_seq.push_back(f);
            _length += f->length();
        } else {
            throw U_EXCEPTION("Input source ",prefix+suffix+(suffix.empty() ? "" : "")," not found.","");
        }
        
    } else if(seq_end == VIDEO_SEQUENCE_UNSPECIFIED_VALUE) {
        std::string base(file::Path(prefix).is_folder() ? "" : file::Path(prefix).filename());
        Print("Trying to find the last file (starting at ", seq_start,") pattern ", base+"%"+Meta::toStr(padding)+"d"+suffix+"."+extension, "...");
        
        _files_in_seq.reserve(10000);
        
        int i =seq_start;
        do {
            std::stringstream ss;
            
            try {
                ss << prefix << std::setfill('0') << std::setw(padding) << i << suffix;
                File *file = File::open(sign_cast<size_t>(i - seq_start), ss.str(), extension);
                if(!file) {
                    break;
                }
                
                _files_in_seq.push_back(file);
                
                _length += file->length();
                
                i++;
                
            } catch (const UtilsException& ex) {
                break;
            }
            
            if(i%10000 == 0)
                Print("Finding file ", i," (",_files_in_seq.size()," found)...");
            if(SETTING(terminate))
                break;
            
        } while (true);
        
        Print("Last number was ", i-1);
        _files_in_seq.shrink_to_fit();
        
    } else {
        Print("Finding all relevant files in sequence with base name ", prefix + (suffix.empty() ? "" : "."+suffix), "...");
        for (int i=seq_start; i<=seq_end; i++) {
            std::stringstream ss;
            ss << prefix << std::setfill('0') << std::setw(padding) << i << suffix;
            
            File *f = File::open(i-seq_start, ss.str(), extension, i != seq_start);
            if(!f)
                throw U_EXCEPTION("Cannot find file ", ss.str() + "." + extension, " in sequence ", seq_start,"-", seq_end, ".");
            _files_in_seq.push_back(f);
            
            _length += f->length();
            
            if(type() != File::Type::IMAGE && i % 50 == 0)
                Print(i, "/", seq_end);
            
            if(SETTING(terminate))
                break;
        }
        
        if (_files_in_seq.empty())
            throw U_EXCEPTION("Provided an empty video sequence for video source ", prefix+(suffix.empty() ? "" : "."+suffix), ".");
    }
    
    if(_files_in_seq.empty())
        throw U_EXCEPTION("Cannot load video sequence ", prefix + (suffix.empty() ? "" : "."+suffix)," (it is empty).");
    
    _size = _files_in_seq.at(0)->resolution();
    _has_timestamps = _files_in_seq.front()->has_timestamps();
    
    /**
     * Because of a bug in older loop bio software, they sometimes report wrong image dimensions in the accompanying npz files ( https://github.com/loopbio/imgstore/issues/12 ).
     * This was because certain file formats only understand powers-of-two image dimensions and the meta-data reported the original image size (before cropping to a valid size). This is solved by & -2, which truncates downwards to even (see https://github.com/mooch443/trex/issues/8 ).
     **/
    if(_has_timestamps) {
        auto& format = _files_in_seq.front()->format();
        if(utils::contains(format, "nvenc-") || utils::contains(format, "264")) {
            // here we can be sure that size has been cropped to even
            _size = cv::Size(_size.width & -2, _size.height & -2);
            
        } else {
            // fallback to less elegant method, try to find out what happened
            auto first = _files_in_seq.front();
            Image image;
            first->frame(_colors, 0_f, image);
            if(image.cols != sign_cast<uint>(_size.width)
               || image.rows != sign_cast<uint>(_size.height))
            {
                FormatWarning("VideoSource ", prefix + (suffix.empty() ? "" : "%d") + suffix," reports resolution ", _size.width, "x", _size.height, " in metadata, but is actually ", image.cols, "x", image.rows, ". Going with the actual video dimensions for now.");
                _size = cv::Size(image.cols, image.rows);
            }
            _last_file = first;
        }
    }
    
    Print("Resolution of VideoSource ", prefix+(suffix.empty() ? "" : "."+suffix), " is ", _size);
    _base = prefix+(suffix.empty() ? "" : "."+suffix);
    
    if(type() == File::VIDEO) {
        _framerate = _files_in_seq.at(0)->framerate();
    } else {
        //! TODO: Frame rate not being set for image sequences...
        //! needs check!
        FormatWarning("No frame rate can be set automatically for a sequence of images. Defaulting to ", framerate(),".");
    }
}

#ifdef USE_GPU_MAT
void VideoSource::frame(Frame_t globalIndex, gpuMat& output, cmn::source_location loc) {
    if (!globalIndex.valid()
        || globalIndex >= _length)
        throw U_EXCEPTION("Invalid frame ", globalIndex, "/", _length, " requested (caller ", loc.file_name(), ":", loc.line(), ")");

    if (type() == File::Type::IMAGE) {
        auto f = _files_in_seq.at(globalIndex.get());
        if (_last_file && _last_file != f)
            _last_file->close();

        _last_file = f;
        f->frame(_colors, 0_f, output, false);

        if (output.empty())
            throw U_EXCEPTION("Could not find frame ", globalIndex, "/", length(), " in VideoSource.");

        return;

    }
    else {
        Frame_t index = 0_f;

        for (auto f : _files_in_seq) {
            if (index + f->length() > globalIndex) {
                if (_last_file && _last_file != f) {
                    _last_file->close();
                }

                _last_file = f;

                /*if (not output.empty()
                    && output.channels() != _buffer.dims)
                {
                    _buffer.create(size().height, size().width, output.channels());
                }*/

                f->frame(_colors, globalIndex - index, output, false);

                    //throw U_EXCEPTION("Could not find frame ", globalIndex, "/", length(), " in VideoSource.");
                //f->frame(_colors, globalIndex - index, output, _lazy_loader);
                //_buffer.get().copyTo(output);
                return;
            }

            index += f->length();
        }

        throw U_EXCEPTION("Could not find frame ", globalIndex, "/", index, " in VideoSource.");
    }
}
#endif

bool VideoSource::frame(Frame_t globalIndex, cv::Mat& output, cmn::source_location loc) {
    if (!globalIndex.valid()
        || globalIndex >= _length)
        throw U_EXCEPTION("Invalid frame ", globalIndex, "/", _length, " requested (caller ", loc.file_name(), ":", loc.line(), ")");

    if (type() == File::Type::IMAGE) {
        auto f = _files_in_seq.at(globalIndex.get());
        if (_last_file && _last_file != f)
            _last_file->close();

        _last_file = f;
        return f->frame(_colors, 0_f, output);

    }
    else {
        Frame_t index = 0_f;

        for (auto f : _files_in_seq) {
            if (index + f->length() > globalIndex) {
                if (_last_file && _last_file != f) {
                    _last_file->close();
                }

                _last_file = f;
                return f->frame(_colors, globalIndex - index, output);
            }

            index += f->length();
        }

        throw U_EXCEPTION("Could not find frame ", globalIndex, "/", index, " in VideoSource.");
    }
}

bool VideoSource::frame(Frame_t globalIndex, Image& output, cmn::source_location loc) {
    if (!globalIndex.valid()
        || globalIndex >= _length)
        throw U_EXCEPTION("Invalid frame ",globalIndex,"/",_length," requested (caller ", loc.file_name(), ":", loc.line(),")");
    
    if(type() == File::Type::IMAGE) {
        auto f = _files_in_seq.at(globalIndex.get());
        if(_last_file && _last_file != f)
            _last_file->close();
        
        _last_file = f;
        return f->frame(_colors, 0_f, output);
        
    } else {
        Frame_t index = 0_f;
        
        for (auto f : _files_in_seq) {
            if (index + f->length() > globalIndex) {
                if (_last_file && _last_file != f) {
                    _last_file->close();
                }
                
                _last_file = f;
                return f->frame(_colors, globalIndex - index, output);
            }
            
            index += f->length();
        }
        
        throw U_EXCEPTION("Could not find frame ",globalIndex,"/",index," in VideoSource.");
    }
}

timestamp_t VideoSource::timestamp(Frame_t globalIndex, cmn::source_location loc) const {
    if (globalIndex.valid() && globalIndex >= _length)
        throw U_EXCEPTION("Invalid frame ",globalIndex,"/",_length," requested (caller ", loc.file_name(), ":", loc.line(),")");
    
    Frame_t index = 0_f;
    
    for (auto f : _files_in_seq) {
        if (index + f->length() > globalIndex) {
            return f->timestamp(globalIndex - index);
        }
        
        index += f->length();
    }
    
    throw U_EXCEPTION("Could not find frame ",globalIndex,"/",index," in VideoSource.");
}

#include <locale>

timestamp_t VideoSource::start_timestamp() const {
    return _files_in_seq.front()->timestamp(0_f);
}

bool VideoSource::has_timestamps() const {
    return false;
    //return _has_timestamps;
}

short VideoSource::framerate() const {
    if(_framerate == -1) {
        if(GlobalSettings::has("frame_rate")) {
            auto frame_rate = SETTING(frame_rate).value<uint32_t>();
            if(frame_rate > 0)
                return frame_rate;
            FormatWarning("frame_rate not set properly for an image sequence. Assuming a default value of 25.");
            return 25;
        }
        throw U_EXCEPTION("Frame rate not set properly in ", *this, " nor in GlobalSettings.");
    }
    return _framerate;
}

void VideoSource::generate_average(cv::Mat &av, uint64_t, std::function<bool(float)>&& callback) {
    gpuMat average;
    av.copyTo(average);
    // if there are only a few files, we can use the standard method
    /*if(_files_in_seq.size() <= 50) {
        GenericVideo::generate_average(average, frameIndex);
        return;
    }*/
    
    // if there are many files, we should use only the first part of each video
    Print("Generating multi-file average...");
    OutputInfo output{
        .channels = static_cast<uint8_t>(_colors == ImageMode::GRAY ? 1u : 3u),
        .encoding = _colors == ImageMode::GRAY ? meta_encoding_t::gray : meta_encoding_t::rgb8
    };
    
    gpuMat float_mat, f, ref;
    std::vector<gpuMat> vec;
    
    averaging_method_t::Class method(averaging_method_t::mean);
    if(GlobalSettings::has("averaging_method"))
        method = SETTING(averaging_method).value<averaging_method_t::Class>();
    //bool use_mean = GlobalSettings::has("averaging_method") && utils::lowercase(SETTING(averaging_method).value<std::string>()) != "max";
    Print("Use averaging method: '", method.name(),"'");
    
    auto [start, end] = [this]() -> std::tuple<Frame_t, Frame_t>{
        if(GlobalSettings::has("video_conversion_range")) {
            auto video_conversion_range = SETTING(video_conversion_range).value<Range<long_t>>();
            return {
                video_conversion_range.start > -1 ? min(Frame_t(video_conversion_range.start), length()) : 0_f,
                video_conversion_range.end > -1 ? min(Frame_t(video_conversion_range.end), length()) : length()
            };
        }
        return {0_f, length()};
    }();
    const auto L = max(start, end) - min(start, end);
    if (L < 10_f) {
        processImage(average, average);
        return;
    }
    
    auto [start_index, end_index] = [&, start=start, end=end]() -> std::tuple<size_t, size_t> {
        size_t start_index{0}, end_index{_files_in_seq.size()-1};
        try {
            Frame_t index = 0_f;
            for(uint64_t i=0; i<_files_in_seq.size(); i++) {
                auto l = _files_in_seq.at(i)->length();
                if(index + l >= start && index < start) {
                    start_index = i;
                }
                if(index + l > end) {
                    end_index = i;
                    break;
                }
                index += l;
            }
        } catch(...) {
            FormatExcept("Failed thread!");
        }
        end_index = saturate(end_index, start_index, _files_in_seq.size());
        return {start_index, end_index};
    }();
    
    AveragingAccumulator acc;
    const Frame_t::number_t N_indexes{narrow_cast<Frame_t::number_t>(end_index - start_index + 1)};
    
    Frame_t::number_t samples = GlobalSettings::has("average_samples") ? (float)SETTING(average_samples).value<uint32_t>() : (L.get() * 0.01f);
    uint64_t step = max(1u, N_indexes < samples
                                ? 1u
                                : (uint64_t)ceil(N_indexes / samples));
    auto frames_per_file =
        max(1_f,
            N_indexes < samples
                ? (L / Frame_t(N_indexes)
                    / (L / Frame_t{samples}))
                : 1_f);
    
    Print("all files=",_files_in_seq.size(), " start_index=",start_index, " end_index=",end_index, " N_indexes=",N_indexes, " samples=",samples, " frames_per_file=",frames_per_file, " step=",step, " start,end=", std::make_tuple(start, end), " L=", L);
    
    if(samples > 255 && method == averaging_method_t::mode)
        throw U_EXCEPTION("Cannot take more than 255 samples with 'averaging_method' = 'mode'. Choose fewer samples or a different averaging method.");
    std::map<File*, std::set<Frame_t>> file_indexes;
    
    Print("generating average in threads step ", step," for ", _files_in_seq.size()," files (", frames_per_file," per file)");
    
    std::mutex mutex;
    GenericThreadPool pool(cmn::hardware_concurrency(), "AverageImage");
    Frame_t index = 0_f;
    for(uint64_t i=start_index; i<=end_index; i+=step) {
        auto file = _files_in_seq.at(i);
        //file_indexes[file].insert(0_f);
        const auto substep = max(1_f, file->length() / frames_per_file);
        for(Frame_t j=start.try_sub(index); j <file->length() && j + index < end; j+=substep) {
            if(j + index >= start) {
                file_indexes[file].insert(j);
            }
        }
        
        index += file->length();
    }
    
    Print("file_indexes = ", file_indexes);
    std::atomic<bool> terminate{false};
    std::vector<std::future<void>> futures;
    std::atomic<size_t> count = 0;
    
    for(auto && [file, indexes] : file_indexes) {
        auto fn = [this, &count, &acc, &callback, samples, &terminate, output](File* file, const std::set<Frame_t>& indexes)
        {
            Image f(size().height, size().width, output.channels);
            
            for(auto index : indexes) {
                try {
                    file->frame(_colors, index, f);
                    assert(f.dims == 1 || f.dims == 3);
                    acc.add_threaded(f.get());
                    //acc.add(f.get());
                    ++count;
                    
                    if(long_t(count) % max(1,long_t(samples * 0.05)) == 0) {
                        if(callback) {
                            if(not callback(double(count.load()) / samples)) {
                                terminate = true;
                                break;
                            }
                        }
                        Print(int(count), " / ", int(samples)," (", file->filename(),")");
                    }
                    
                } catch (const UtilsException& e) {
                    FormatWarning("Continuing, but caught an exception processing frame ", index," of '", file->filename(),"' while generating an average.");
                }
                
                //if(SETTING(terminate))
                //    return;
            }
            
            file->close();
        };
        
        if(file_indexes.size() > 1) {
            futures.emplace_back(pool.enqueue(fn, file, indexes));
        } else
            fn(file, indexes);
        
        if(terminate)
            break;
    }
    
    //pool.wait();
    for(auto & f : futures)
        f.get();
    
    _last_file = NULL;
    
    auto image = acc.finalize();
    auto mat = image->get();
    
    if(_colors == ImageMode::R3G3B2) {
        cv::Mat output;
        convert_to_r3g3b2<3>(mat, output);
        output.copyTo(av);
        assert(mat.type() == CV_8UC(mat.channels()));
        return;
    }
    
    assert(mat.type() == CV_8UC(mat.channels()));
    mat.copyTo(av);
}

}
