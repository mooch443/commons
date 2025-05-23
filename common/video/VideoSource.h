#ifndef _VIDEO_SOURCE_H
#define _VIDEO_SOURCE_H

#include "types.h"
#include <video/GenericVideo.h>
#include <file/Path.h>
#include <video/Video.h>
#include <file/PathArray.h>
#include <video/FFmpegVideoCapture.h>

#define VIDEO_SEQUENCE_INVALID_VALUE (-1)
#define VIDEO_SEQUENCE_UNSPECIFIED_VALUE (-2)

namespace cmn {
    class Video;
    class VideoSource;
}

class cmn::VideoSource : public cmn::GenericVideo {
public:
    
public:
    class File {
    public:
        enum Type {
            UNKNOWN,
            VIDEO,
            IMAGE
        };
        
        static File *open(size_t index, const std::string& basename, const std::string& ext, bool no_check = false);
        static std::string complete_name(const std::string& basename, const std::string& ext);
        
        File(size_t index, file::Path path, Frame_t length, Size2 size, uint32_t frame_rate, Type type, bool is_greyscale);
        
    private:
        static std::vector<std::pair<std::string, Type>> _extensions;
        
        GETTER(size_t, index);
        GETTER(std::string, filename);
        Frame_t _length;
        //Video *_video;
        FfmpegVideoCapture *_video{nullptr};
        Type _type;
        std::optional<uint32_t> _frame_rate;
        std::optional<bool> _is_greyscale;
        
        GETTER(std::string, format);
        
        std::vector<double> _timestamps;
        cv::Size _size;
        
    private:
        File(size_t index, const std::string& basename, const std::string& extension);
        File(const File&) = delete;
    public:
        File(File&&);
        
        std::set<std::string_view> recovered_errors() const {
            return not _video ? std::set<std::string_view>{} : _video->recovered_errors();
        }
        
    public:
        ~File();
        auto length() const { return _length; }
        const cv::Size& resolution();
        bool is_greyscale();
        
        void frame(cmn::ImageMode color, Frame_t frameIndex, gpuMat& output, bool lazy_video = false, cmn::source_location loc = cmn::source_location::current()) const;
        bool frame(cmn::ImageMode color, Frame_t frameIndex, cv::Mat& output, cmn::source_location loc = cmn::source_location::current()) const;
        bool frame(cmn::ImageMode color, Frame_t frameIndex, Image& output, cmn::source_location loc = cmn::source_location::current()) const;
        void close() const;
        Type type() const { return _type; }
        bool has_timestamps() const;
        timestamp_t timestamp(Frame_t frameIndex, cmn::source_location loc = cmn::source_location::current()) const;
        short framerate();
    };
    
private:
    /**
     * (Video) files
     */
    std::vector<File*> _files_in_seq;
    
    GETTER(std::string, source);
    GETTER(file::Path, base);
    File* _last_file = nullptr;
    cv::Size _size;
    Frame_t _length = 0_f;
    cv::Mat _average;
    cv::Mat _mask;
    bool _has_timestamps = false;
    short _framerate = -1;
    Image _buffer;
    GETTER(bool, is_greyscale){false};
    
private:
    GETTER_SETTER_I(ImageMode, colors, ImageMode::GRAY);
    GETTER_SETTER_I(bool, lazy_loader, false);
    
public:
    /**
     * Automatically load a range of files with a certain extension called
     * {basename}{seq_start<=number<=seq_end}.{extension}
     */
    VideoSource() = default;
    VideoSource(VideoSource&&);
    VideoSource(const VideoSource&) = delete;
    VideoSource& operator=(VideoSource&&) = default;
    VideoSource& operator=(const VideoSource&) = delete;
    
    VideoSource(const file::PathArray& source);
    void open(const std::string& prefix, const std::string& suffix, const std::string& extension, int seq_start = VIDEO_SEQUENCE_INVALID_VALUE, int seq_end = VIDEO_SEQUENCE_INVALID_VALUE, int padding = 4);
    
    ~VideoSource();
    
    /**
     * ### GENERICVIDEO INTERFACE ###
     **/
#ifdef USE_GPU_MAT
    void frame(Frame_t globalIndex, gpuMat& output, cmn::source_location loc = cmn::source_location::current()) override;
#endif
    using cmn::GenericVideo::frame;
    bool frame(Frame_t globalIndex, cv::Mat& output, cmn::source_location loc = cmn::source_location::current());
    bool frame(Frame_t globalIndex, Image& output, cmn::source_location loc = cmn::source_location::current()) override;
    const cv::Size& size() const override { return _size; }
    Frame_t length() const override { return _length; }
    const cv::Mat& average() const override { return _average; }
    cv::Mat& average() { return _average; }
    bool supports_multithreads() const override { return type() == File::Type::IMAGE; }
    
    File::Type type() const { if(_files_in_seq.empty()) return File::Type::UNKNOWN; return _files_in_seq.at(0)->type(); }
    
    virtual bool has_timestamps() const override;
    virtual timestamp_t timestamp(Frame_t, cmn::source_location loc = cmn::source_location::current()) const override;
    virtual timestamp_t start_timestamp() const override;
    
    virtual short framerate() const override;
    
    virtual bool has_mask() const override { return false; }
    virtual const cv::Mat& mask() const override { return _mask; }
    
    virtual void generate_average(cv::Mat &average, uint64_t frameIndex, std::function<bool(float)>&& callback = nullptr) override;
    
    virtual std::string toStr() const;
    
    std::set<std::string_view> recovered_errors() const {
        for(auto file : _files_in_seq) {
            if(file && file->type() == File::Type::VIDEO) {
                return file->recovered_errors();
            }
        }
        return {};
    }
};

#endif
