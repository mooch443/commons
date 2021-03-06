#include "VideoSource.h"
#include "Video.h"
#include <regex>
#include <file/Path.h>
#include <misc/GlobalSettings.h>
#include <misc/ThreadPool.h>
#include <misc/checked_casts.h>
#include <misc/Image.h>
#include <video/AveragingAccumulator.h>

using namespace cmn;

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

VideoSource::File::File(size_t index, const std::string& basename, const std::string& extension) : _index(index), _video(NULL), _size(0, 0) {
    _filename = complete_name(basename, extension);
    
    // check the extension(-type)
    _type = UNKNOWN;
    for (auto ext : _extensions) {
        if (ext.first == utils::lowercase(extension)) {
            _type = ext.second;
            break;
        }
    }
    
    if (_type == UNKNOWN)
        throw U_EXCEPTION("Unknown extension '",extension,"' for file '",_filename,"'.");
    
    switch (_type) {
        case VIDEO: {
            _video = new Video();
            auto npz = file::Path(_filename).replace_extension("npz");
            if(npz.exists()) {
                try {
                    static bool message = false;
                    if(!message) {
                        print("Found timestamps for file ", npz.str(),".");
                        message = true;
                    }
                    
                    _format = load_string(npz, "format");
                    _timestamps = cnpy::npz_load(npz.str(), "frame_time").as_vec<double>();
                    auto res = cnpy::npz_load(npz.str(), "imgshape").as_vec<int64_t>();
                    _size = cv::Size( (int)res[1], (int)res[0] );
                    _length = _timestamps.size();
                } catch(...) {
                    FormatExcept("Failed opening NPZ archive ",npz.str()," with (presumably) timestamps in them for video ",_filename,". Proceeding without.");
                    
                    if(!_video->open(_filename))
                        throw U_EXCEPTION("Opening Video ",_filename," failed.");
                    _length = _video->length();
                    _video->close();
                }
                
            } else {
                if(!_video->open(_filename))
                    throw U_EXCEPTION("Opening Video ",_filename," failed.");
                _length = _video->length();
                _video->close();
            }
            break;
        }
            
        case IMAGE:
            _length = 1u;
            break;
            
        default:
            break;
    }
}

void VideoSource::File::frame(long_t frameIndex, cv::Mat& output, bool lazy_video) const {
    assert(output.cols == _video->size().width
           && output.rows == _video->size().height);
    
    switch (_type) {
        case VIDEO:
            if (!_video->isOpened())
                _video->open(_filename);
            if (!_video->isOpened())
                throw U_EXCEPTION("Video ",_filename," cannot be opened.");
            _video->frame(frameIndex, output, lazy_video);
            break;
            
        case IMAGE:
            cv::imread(_filename, cv::IMREAD_GRAYSCALE).copyTo(output);
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
    
    if(has_timestamps()) {
        if(_timestamps.size() > 1) {
            auto prev = _timestamps[0];
            double average = 0;
            for (uint64_t i=1; i<_timestamps.size(); i++) {
                average += _timestamps[i] - prev;
                prev = _timestamps[i];
            }
            average = average / double(_timestamps.size()-1);
            return (short)round(1. / average);
        } else
            return -1;
        
    } else {
        if(!_video->isOpened())
            _video->open(_filename);
        auto fps = _video->framerate();
        _video->close();
        return narrow_cast<short>(fps);
    }
}

timestamp_t VideoSource::File::timestamp(uint64_t frameIndex) const {
    if(_type != VIDEO)
        throw U_EXCEPTION("Cannot retrieve timestamps from anything else other than videos.");
    
    if(!has_timestamps())
        throw U_EXCEPTION("No timestamps available for ",_filename,".");
    
    auto times = _timestamps[frameIndex];
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
            case VIDEO:
                if(!_video->isOpened())
                    _video->open(_filename);
                _size = _video->size();
                break;
                
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

VideoSource::VideoSource(const std::string& source) {
    std::smatch m;
    std::regex rplaceholder ("%[0-9]+(\\.[0-9]+(.[1-9][0-9]*)?)?d");
    std::regex rext (".*(\\..+)$");
    
    long_t number_length = -1, start_number = 0, end_number = VIDEO_SEQUENCE_UNSPECIFIED_VALUE;
    
    std::string prefix, suffix, extension;
    if(std::regex_search(source,m,rext)) {
        auto x = m[1];
        extension = x.str().substr(1);
        prefix = source.substr(0u, (uint64_t)m.position(1));
        
        print("Extension ",extension," basename ",prefix);
        
    } else {
        throw U_EXCEPTION("File extension not found in ",source);
    }
    
    print("Searching for ",prefix);
    if(std::regex_search (prefix,m,rplaceholder)) {
        auto x = m[0];
        auto s = x.str();
        print("Match ",s);
        
        auto L = s.length();
        auto p = (uint64_t)m.position();
        
        s = s.substr(1, s.length()-2);
        auto split = utils::split(s, '.');
        
        if(split.size()>1) {
            start_number = std::stoi(split[1]);
        }
        if(split.size()>2) {
            end_number = std::stoi(split[2]);
        }
        
        number_length = std::stoi(split[0]);
        suffix = prefix.substr(p + L);
        prefix = prefix.substr(0u, p);
        print("match ",s," at ",p," with nr ",number_length,". suffix = ", suffix);
    }
    
    if(number_length != -1) {
        // no placeholders found, just load file.
        open(prefix, suffix, extension, start_number, end_number, number_length);
    } else {
        open(prefix, suffix, extension);
    }
}

VideoSource::VideoSource(const std::vector<file::Path>& files)
{
    for(auto &path : files) {
        auto extension = std::string(path.extension());
        auto basename = path.remove_extension().str();
        auto f = File::open(_files_in_seq.size(), basename, extension);
        if(!f)
            throw U_EXCEPTION("Cannot open file ",path.str(),".");
        
        _files_in_seq.push_back(f);
        _length += f->length();
    }
    
    if(_files_in_seq.empty()) {
        auto str = Meta::toStr(files);
        throw U_EXCEPTION("Cannot load video sequence ",str," (it is empty).");
    }
    
    _size = _files_in_seq.at(0)->resolution();
    _has_timestamps = _files_in_seq.front()->has_timestamps();
}

VideoSource::VideoSource() {}
void VideoSource::open(const std::string& prefix, const std::string& suffix, const std::string& extension, int seq_start, int seq_end, int padding)
{
    if (seq_start == VIDEO_SEQUENCE_INVALID_VALUE || seq_end == VIDEO_SEQUENCE_INVALID_VALUE) {
        File *f = File::open(0, prefix + suffix, extension);

        if(f && f->type() != File::VIDEO)
            FormatWarning("Just loading one image because seq_end/seq_start were not specified.");

        if(f) {
            _files_in_seq.push_back(f);
            _length += f->length();
        } else {
            throw U_EXCEPTION("Input source '",prefix,"",suffix,"",suffix.empty() ? "" : "","' not found.","");
        }
        
    } else if(seq_end == VIDEO_SEQUENCE_UNSPECIFIED_VALUE) {
        std::string base(file::Path(prefix).is_folder() ? "" : file::Path(prefix).filename());
        print("Trying to find the last file (starting at ", seq_start,") pattern ", base+"%"+Meta::toStr(padding)+"d"+suffix+"."+extension, "...");
        
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
                print("Finding file ", i," (",_files_in_seq.size()," found)...");
            if(SETTING(terminate))
                break;
            
        } while (true);
        
        print("Last number was ", i-1);
        _files_in_seq.shrink_to_fit();
        
    } else {
        print("Finding all relevant files in sequence with base name '", prefix.c_str(), suffix.empty() ? "" : "",suffix.c_str(), "'...");
        for (int i=seq_start; i<=seq_end; i++) {
            std::stringstream ss;
            ss << prefix << std::setfill('0') << std::setw(padding) << i << suffix;
            
            File *f = File::open(i-seq_start, ss.str(), extension, i != seq_start);
            if(!f)
                throw U_EXCEPTION("Cannot find file '", ss.str().c_str(), ".", extension.c_str(), "' in sequence ", seq_start,"-", seq_end, ".");
            _files_in_seq.push_back(f);
            
            _length += f->length();
            
            if(type() != File::Type::IMAGE && i % 50 == 0)
                print(i, "/", seq_end);
            
            if(SETTING(terminate))
                break;
        }
        
        if (_files_in_seq.empty())
            throw U_EXCEPTION("Provided an empty video sequence for video source '", prefix.c_str(), suffix.empty() ? "" : "",suffix.c_str(),"", "'.");
    }
    
    if(_files_in_seq.empty())
        throw U_EXCEPTION("Cannot load video sequence '", prefix.c_str(), suffix.empty() ? "" : "",suffix.c_str(),"", "' (it is empty).");
    
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
            cv::Mat image;
            first->frame(0, image);
            if(image.cols != _size.width || image.rows != _size.height) {
                FormatWarning("VideoSource '", prefix.c_str(), suffix.empty() ? "" : "%d", suffix.c_str(),"' reports resolution ", _size.width, "x", _size.height, " in metadata, but is actually ", image.cols, "x", image.rows, ". Going with the actual video dimensions for now.");
                _size = cv::Size(image.cols, image.rows);
            }
            _last_file = first;
        }
    }
    
    print("Resolution of VideoSource ", prefix+(suffix.empty() ? "" : "."+suffix), " is ", _size);
    
    if(type() == File::VIDEO) {
        _framerate = _files_in_seq.at(0)->framerate();
    }
}

#ifdef USE_GPU_MAT
void VideoSource::frame(uint64_t globalIndex, gpuMat& output) {
    cv::Mat m(size().height, size().width, CV_8UC1);
    frame(globalIndex, m);
    m.copyTo(output);
}
#endif

void VideoSource::frame(uint64_t globalIndex, cv::Mat& output) {
    if (/*globalIndex < 0 ||*/ globalIndex >= _length)
        throw U_EXCEPTION("Invalid frame ",globalIndex,"/",_length," requested.");
    
    if(type() == File::Type::IMAGE) {
        auto f = _files_in_seq.at(globalIndex);
        if(_last_file && _last_file != f)
            _last_file->close();
        
        _last_file = f;
        f->frame(0, output);
        
        if(output.empty())
            throw U_EXCEPTION("Could not find frame ",globalIndex,"/",length()," in VideoSource.");
        
        return;
        
    } else {
        uint64_t index = 0;
        
        for (auto f : _files_in_seq) {
            if (index + f->length() > globalIndex) {
                if (_last_file && _last_file != f) {
                    _last_file->close();
                }
                
                _last_file = f;
                f->frame(globalIndex-index, output);
                
                if(output.empty())
                    throw U_EXCEPTION("Could not find frame ",globalIndex,"/",length()," in VideoSource.");
                
                return;
            }
            
            index += f->length();
        }
        
        throw U_EXCEPTION("Could not find frame ",globalIndex,"/",index," in VideoSource.");
    }
}

timestamp_t VideoSource::timestamp(uint64_t globalIndex) const {
    if (/*globalIndex < 0 ||*/ globalIndex >= _length)
        throw U_EXCEPTION("Invalid frame ",globalIndex,"/",_length," requested.");
    
    uint64_t index = 0;
    
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
    return _files_in_seq.front()->timestamp(0);
}

bool VideoSource::has_timestamps() const {
    return _has_timestamps;
}

short VideoSource::framerate() const {
    return _framerate;
}

void VideoSource::generate_average(cv::Mat &av, uint64_t, std::function<void(float)>&& callback) {
    gpuMat average;
    av.copyTo(average);
    // if there are only a few files, we can use the standard method
    /*if(_files_in_seq.size() <= 50) {
        GenericVideo::generate_average(average, frameIndex);
        return;
    }*/
    
    // if there are many files, we should use only the first part of each video
    print("Generating multi-file average...");
    
    gpuMat float_mat, f, ref;
    std::vector<gpuMat> vec;
    
    averaging_method_t::Class method(averaging_method_t::mean);
    if(GlobalSettings::has("averaging_method"))
        method = SETTING(averaging_method).value<averaging_method_t::Class>();
    //bool use_mean = GlobalSettings::has("averaging_method") && utils::lowercase(SETTING(averaging_method).value<std::string>()) != "max";
    print("Use averaging method: '", method.name(),"'");
    if (length() < 10) {
        processImage(average, average);
        return;
    }
    
    AveragingAccumulator acc;
    
    float samples = GlobalSettings::has("average_samples") ? (float)SETTING(average_samples).value<uint32_t>() : (length() * 0.01f);
    uint64_t step = max(1u, _files_in_seq.size() < samples ? 1u : (uint64_t)ceil(_files_in_seq.size() / samples));
    uint64_t frames_per_file = max(1, _files_in_seq.size() < samples ? (length() / _files_in_seq.size()) / (length() / samples) : 1);
    
    if(samples > 255 && method == averaging_method_t::mode)
        throw U_EXCEPTION("Cannot take more than 255 samples with 'averaging_method' = 'mode'. Choose fewer samples or a different averaging method.");
    std::map<File*, std::set<uint64_t>> file_indexes;
    
    print("generating average in threads step ", step," for ", _files_in_seq.size()," files (", frames_per_file," per file)");
    
    std::mutex mutex;
    GenericThreadPool pool(cmn::hardware_concurrency(), [](auto e) { std::rethrow_exception(e); }, "AverageImage");
    for(uint64_t i=0; i<_files_in_seq.size(); i+=step) {
        auto file = _files_in_seq.at(i);
        file_indexes[file].insert(0);
        if(frames_per_file > 1) {
            auto step = max(1u, (uint64_t)ceil((uint64_t)file->length() / frames_per_file));
            for(uint64_t i=step; i<(uint64_t)file->length(); i+= step) {
                file_indexes[file].insert(i);
            }
        }
    }
    
    for(auto && [file, indexes] : file_indexes) {
        auto fn = [&acc, &callback, samples](File* file, const std::set<uint64_t>& indexes)
        {
            cv::Mat f;
            double count = 0;
            
            for(auto index : indexes) {
                try {
                    file->frame(index, f, true);
                    assert(f.channels() == 1);
                    acc.add_threaded(f);
                    ++count;
                    
                    if(long_t(count) % max(1,long_t(samples * 0.1)) == 0) {
                        if(callback) {
                            callback(count / samples);
                        }
                        print(int(count), " / ", int(samples)," (", file->filename(),")");
                    }
                    
                } catch (const UtilsException& e) {
                    FormatWarning("Continuing, but caught an exception processing frame ", index," of '", file->filename(),"' while generating an average.");
                }
                
                if(SETTING(terminate))
                    return;
            }
            
            file->close();
        };
        
        if(file_indexes.size() > 1) {
            pool.enqueue(fn, file, indexes);
        } else
            fn(file, indexes);
    }
    
    pool.wait();
    _last_file = NULL;
    
    auto image = acc.finalize();
    auto mat = image->get();
    assert(mat.type() == CV_8UC1);
    mat.copyTo(av);
}
