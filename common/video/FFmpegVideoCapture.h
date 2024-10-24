#pragma once

#include <commons.pc.h>
#include <misc/Image.h>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/frame.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
    #include <libavutil/hwcontext.h>
    #include <libavutil/timestamp.h>
}

namespace cmn {

class FfmpegVideoCapture {
    std::unique_ptr<cv::VideoCapture> _capture;
    mutable std::set<std::string_view> _recovered_errors;
    std::once_flag skip_message_flag;
    static std::unordered_map<std::string, int64_t> tested_video_lengths;
    static std::mutex tested_video_lengths_mutex;
    
public:
    FfmpegVideoCapture(const std::string& filePath);
    ~FfmpegVideoCapture();

    std::set<std::string_view> recovered_errors() const { return _recovered_errors; }
    bool is_open() const;
    int64_t length() const;
    Size2 dimensions() const;
    int channels() const;
    int frame_rate() const;
    
    bool read(uint32_t frameIndex, Image& frame);
    bool read(uint32_t frameIndex, gpuMat& frame);
    bool read(uint32_t frameIndex, cv::Mat& frame);
    
    void close();
    bool open(const std::string& filePath);

private:
    AVFormatContext* formatContext = nullptr;
    AVCodecContext* codecContext = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* pkt = nullptr;
    
    AVPixelFormat swsFormat = AVPixelFormat::AV_PIX_FMT_NONE;
    AVPixelFormat swsInputFormat = AVPixelFormat::AV_PIX_FMT_RGBA;
    SwsContext* swsContext = nullptr;
    
    AVBufferRef* hw_device_ctx = nullptr;
    AVFrame *sw_frame = nullptr;
    
    Image _buffer;

    int videoStreamIndex = -1;
    int64_t current_frame = -1;
    int64_t actual_frame_count_ = -1;
    
    std::string _filePath;

    bool seek_frame(uint32_t frameIndex);
    bool transfer_frame_to_software(AVFrame* frame);

    template<typename Mat>
    bool convert_frame_to_mat(const AVFrame* frame, Mat&);

    bool update_sws_context(const AVFrame* frame, int dims = 0);

    template<typename Mat>
    bool _read(uint32_t frameIndex, Mat& mat);
    
    template<typename Mat>
    bool decode_frame(Mat& mat);
    
private:
    void recovered_error(const std::string_view&) const;
    void log_packet(const AVPacket *pkt);
    static std::string error_to_string(int);
    
    static int64_t calculate_actual_frame_count(FfmpegVideoCapture&, const file::Path& path);
    static int64_t get_actual_frame_count(FfmpegVideoCapture&, const file::Path& path);
    int64_t estimated_frame_count() const;
};

} // namespace cmn
