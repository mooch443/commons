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
}

namespace cmn {

class FfmpegVideoCapture {
    //Size2 _dimensions;
    //int _channels{-1};
    std::unique_ptr<cv::VideoCapture> _capture;
    //std::map<int64_t, int64_t> _keyframes;
    bool _disable_jumping{false};
    mutable std::set<std::string_view> _recovered_errors;
    std::once_flag skip_message_flag;
    
public:
    FfmpegVideoCapture(const std::string& filePath);
    ~FfmpegVideoCapture();

    std::set<std::string_view> recovered_errors() const { return _recovered_errors; }
    bool is_open() const;
    int64_t length() const;
    Size2 dimensions() const;
    int channels() const;
    int frame_rate() const;
    bool set_frame(uint32_t index);
    bool read(Image& frame);
    bool read(gpuMat& frame);
    bool read(cv::Mat& frame);
    bool grab();
    
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
    int64_t frameCount = 0;
    
    std::string _filePath;

    bool seek_frame(uint32_t frameIndex);
    bool transfer_frame_to_software(AVFrame* frame);

    template<typename Mat>
    bool convert_frame_to_mat(const AVFrame* frame, Mat&);

    bool update_sws_context(const AVFrame* frame, int dims = 0);

    template<typename Mat>
    bool _read(Mat& mat);
    
private:
    void recovered_error(const std::string_view&) const;
};

} // namespace cmn
