#pragma once

#include <commons.pc.h>
#include <misc/Image.h>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/frame.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

namespace cmn {

class FfmpegVideoCapture {
    //Size2 _dimensions;
    //int _channels{-1};
    
public:
    FfmpegVideoCapture(const std::string& filePath);
    ~FfmpegVideoCapture();

    bool is_open() const;
    int64_t length() const;
    Size2 dimensions() const;
    int channels() const;
    int frame_rate() const;
    bool set_frame(uint32_t index);
    bool read(Image& frame);
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
    bool convert_frame_to_mat(const AVFrame* frame, Image& outFrame);
    bool update_sws_context(const AVFrame* frame, int dims = 0);
};

} // namespace cmn
