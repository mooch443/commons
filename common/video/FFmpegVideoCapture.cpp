#include "FFmpegVideoCapture.h"
#include <misc/format.h>
#include <misc/Timer.h>

//#undef NDEBUG

namespace cmn {

FfmpegVideoCapture::FfmpegVideoCapture(const std::string& filePath) {
    avformat_network_init();
    if (not filePath.empty()
        && not open(filePath)) {
        // Handle error if opening the file fails
        std::cerr << "[FFMPEG] Failed to open video file: " << filePath << std::endl;
    }
    av_log_set_level(AV_LOG_QUIET);
    //_disable_jumping = true;
}

bool FfmpegVideoCapture::open(const std::string& filePath) {
    //av_log_set_level(AV_LOG_DEBUG);
    // Allocate format context
    formatContext = avformat_alloc_context();
    if (not formatContext) {
        FormatError("[FFMPEG] Failed to allocate format context");
        return false;
    }

    //_capture = std::make_unique<cv::VideoCapture>(filePath);

    // Open video file
    if (avformat_open_input(&formatContext, filePath.c_str(), nullptr, nullptr) != 0) {
        FormatError("[FFMPEG] Failed to open video file: ", filePath);
        return false;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        FormatError("[FFMPEG] Failed to find stream information");
        return false;
    }
    
    // Find the first video stream
    for (unsigned i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        FormatError("[FFMPEG] Failed to find video stream");
        return false;
    }

    // Calculate duration in seconds
    AVStream *videoStream = formatContext->streams[videoStreamIndex];
    double durationInSeconds = videoStream->duration * av_q2d(videoStream->time_base);

    std::cout << "Duration: " << durationInSeconds << " seconds" << std::endl;
    std::cout << "nb_frames: " << videoStream->nb_frames << std::endl;
    std::cout << "av_q2d(videoStream->time_base): " << av_q2d(videoStream->time_base)<< " " << videoStream->time_base.num << "/" << videoStream->time_base.den << std::endl;
    
#ifndef NDEBUG
    AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_NONE;
    while ((hw_type = av_hwdevice_iterate_types(hw_type)) != AV_HWDEVICE_TYPE_NONE) {
        Print("[FFMPEG] HW type: ", hw_type);
    }
#endif

    int hw_start_index = 0;
retry_codec:
    //! we will only accept hardware acceleration types that we know are good for us
    AVHWDeviceType chosenHWType = AV_HWDEVICE_TYPE_NONE;

#if defined(WIN32)
    static constexpr std::array preferred_devices{
        AV_HWDEVICE_TYPE_CUDA,
        AV_HWDEVICE_TYPE_D3D11VA,
        AV_HWDEVICE_TYPE_DXVA2,
        AV_HWDEVICE_TYPE_QSV
    };
#elif __APPLE__
    static constexpr std::array preferred_devices{
        AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
        AV_HWDEVICE_TYPE_OPENCL
    };
#else
    static constexpr std::array preferred_devices{
        AV_HWDEVICE_TYPE_CUDA,
        AV_HWDEVICE_TYPE_OPENCL,
        AV_HWDEVICE_TYPE_VAAPI,
        AV_HWDEVICE_TYPE_MEDIACODEC,
        AV_HWDEVICE_TYPE_DXVA2
    };
#endif
    
    if(hw_device_ctx) {
        // Don't forget to free the device context before trying to use it
        av_buffer_unref(&hw_device_ctx);
        hw_device_ctx = nullptr;
    }

    int i = 0;
    for(auto hw_type : preferred_devices) {
        if(hw_start_index > i) {
			++i;
			continue;
		}

        ++i;

        if (av_hwdevice_ctx_create(&hw_device_ctx, hw_type, nullptr, nullptr, 0) == 0) {
            chosenHWType = hw_type;
            break;
        } else {
            assert(not hw_device_ctx);
        }
    }

    hw_start_index = i;

    if (hw_device_ctx == nullptr) {
        FormatWarning("[FFMPEG] Failed to create a hardware device context: ", preferred_devices);
    }

    /*if(hw_device_ctx == nullptr) {
        while ((hw_type = av_hwdevice_iterate_types(hw_type)) != AV_HWDEVICE_TYPE_NONE) {
            // Check if this hardware type is supported by the codec
            // You can also apply your own prioritization logic here
            if (av_hwdevice_ctx_create(&hw_device_ctx, hw_type, nullptr, nullptr, 0) < 0) {
                FormatWarning("Failed to create a hardware device context: ", hw_type);
            } else
                break;
        }
    }*/
    
    // Get a pointer to the codec for the video stream
    AVCodecParameters* codecParameters = formatContext->streams[videoStreamIndex]->codecpar;
    const AVCodec* codec{nullptr};
    if (chosenHWType == AV_HWDEVICE_TYPE_CUDA) {
        if(codecParameters->codec_id == AV_CODEC_ID_H264)
            codec = avcodec_find_decoder_by_name("h264_cuvid");
        else if(codecParameters->codec_id == AV_CODEC_ID_HEVC)
            codec = avcodec_find_decoder_by_name("hevc_cuvid");
    }
    if(not codec)
        codec = avcodec_find_decoder(codecParameters->codec_id);

    if (!codec) {
        FormatError("[FFMPEG] Unsupported codec id: ", codecParameters->codec_id);
        return false;
    }

    // ffmpeg -y -vsync 0 -hwaccel cuda -hwaccel_output_format cuda -i input.mp4 -c:a copy -c:v h264_nvenc -preset p6 -tune ll -b:v 5M -bufsize 5M -maxrate 10M -qmin 0 -g 250 -bf 3 -b_ref_mode middle -temporal-aq 1 -rc-lookahead 20 -i_qfactor 0.75 -b_qfactor 1.1 output.mp4
    // Allocate codec context
    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        FormatError("[FFMPEG] Failed to allocate codec context");
        return false;
    }

    // Copy codec parameters to codec context
    if (avcodec_parameters_to_context(codecContext, codecParameters) < 0) {
        FormatError("[FFMPEG] Failed to copy codec parameters to codec context");
        return false;
    }
    
    if(hw_device_ctx) {
        // Step 3: Assign the hardware context to the codec context
        codecContext->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    }
    
    // Open codec
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        avcodec_free_context(&codecContext);

        codecContext = nullptr;
        codec = nullptr;
        if(sign_cast<size_t>(hw_start_index) <= preferred_devices.size()) {
			FormatWarning("[FFMPEG] Failed to open codec with hardware acceleration. Retrying with next hardware device.");
			goto retry_codec;
		}

        FormatError("[FFMPEG] Failed to open codec");
        return false;
    }
    
    // Allocate frame and packet
    frame = av_frame_alloc();
    pkt = av_packet_alloc();
    if (!frame || !pkt) {
        FormatError("[FFMPEG] Failed to allocate frame or packet");
        return false;
    }

    _filePath = filePath;
    return true;
}

bool FfmpegVideoCapture::is_open() const {
    return formatContext != nullptr;
}

void FfmpegVideoCapture::recovered_error(const std::string_view& str) const {
    if(not _recovered_errors.contains(str))
        _recovered_errors.insert(str);
}

int64_t FfmpegVideoCapture::length() const {
    if (!is_open()) return -1;
    
    AVStream *videoStream = videoStreamIndex < int64_t(formatContext->nb_streams)
        ? formatContext->streams[videoStreamIndex]
        : nullptr;
    if(not videoStream)
        return 0;
    
    int64_t N = formatContext->streams[videoStreamIndex]->nb_frames;
    if(N <= 0) {
        static constexpr std::string_view msg{"The video-stream does not provide exact length information."};
        recovered_error(msg);
        
        double durationInSeconds = videoStream->duration * av_q2d(videoStream->time_base);
        auto fr = frame_rate();
        if(fr <= 0)
            return -1;
        return ceil(durationInSeconds * float(fr));
    }
    return N;
}

Size2 FfmpegVideoCapture::dimensions() const {
    if (!is_open()) return Size2();
    return Size2{
        (Float2_t)codecContext->width,
        (Float2_t)codecContext->height
    };
}

int FfmpegVideoCapture::frame_rate() const {
    if (!is_open()) return -1;
    if(codecContext->framerate.num <= 0) {
        static constexpr std::string_view msg{"The video-stream does not provide frame-rate information."};
        
        if(formatContext
           && formatContext->streams[ videoStreamIndex ]->avg_frame_rate.num > 0)
        {
            //1298000/103809
            auto fr = static_cast<int>(float(formatContext->streams[ videoStreamIndex ]->avg_frame_rate.num) / formatContext->streams[ videoStreamIndex ]->avg_frame_rate.den);
            if(fr > 0) {
                recovered_error(msg);
                return fr;
            }
        }
        
        if(formatContext
           && formatContext->streams[ videoStreamIndex ]->r_frame_rate.num > 0)
        {
            //1298000/103809
            auto fr = static_cast<int>(float(formatContext->streams[ videoStreamIndex ]->r_frame_rate.num) / formatContext->streams[ videoStreamIndex ]->r_frame_rate.den);
            if(fr > 0) {
                recovered_error(msg);
                return fr;
            }
        }
        
        //! cannot retrieve frame rate
        return -1;
    }
    return codecContext->framerate.num / codecContext->framerate.den;
}

int FfmpegVideoCapture::channels() const {
    // Assuming video frames are in a standard format, they have 3 channels (RGB or YUV)
    return swsInputFormat == AV_PIX_FMT_RGBA
        ? 4
        : (swsInputFormat == AV_PIX_FMT_RGB24 ? 3 : 1);
}

void FfmpegVideoCapture::log_packet(const AVPacket *pkt) {
#if !defined(NDEBUG) && !defined(__linux__)
    AVRational *time_base = &formatContext->streams[pkt->stream_index]->time_base;
    std::cout << "Packet - pts:" << av_ts2str(pkt->pts) << " pts_time:" << av_ts2timestr(pkt->pts, time_base)
              << " dts:" << av_ts2str(pkt->dts) << " dts_time:" << av_ts2timestr(pkt->dts, time_base)
              << " duration:" << pkt->duration << " duration_time:" << av_ts2timestr(pkt->duration, time_base)
              << " stream_index:" << pkt->stream_index << std::endl;
#endif
}

std::string FfmpegVideoCapture::error_to_string(int response) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(response, errbuf, sizeof(errbuf));
    return std::string(errbuf);
}

bool FfmpegVideoCapture::seek_frame(uint32_t frameIndex) {
    static Timing timing("ffmpeg::seek_frame");
    TakeTiming take(timing);
    
    // If the desired frame is already the current frame, no need to seek
    //if(frameCount == frameIndex)
    //    return true;
    
    AVRational time_base = formatContext->streams[videoStreamIndex]->time_base;
    AVRational frame_rate = av_guess_frame_rate(formatContext, formatContext->streams[videoStreamIndex], nullptr);

    // Calculate the keyframe interval
    int keyframe_interval = formatContext->streams[videoStreamIndex]->avg_frame_rate.num / formatContext->streams[videoStreamIndex]->avg_frame_rate.den;
    
    int64_t received_frame = -1;
    
    // Determine if we should seek or decode sequentially
    if (frameCount < static_cast<int64_t>(frameIndex)
        && abs(static_cast<int64_t>(frameIndex) - frameCount) <= keyframe_interval)
    {
        received_frame = frameCount;
        
    } else {
        // If far away, seek directly to the calculated timestamp
        int64_t timestamp = av_rescale_q(max(0, static_cast<int64_t>(frameIndex)), av_inv_q(frame_rate), time_base);

        if (av_seek_frame(formatContext, videoStreamIndex, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
            FormatExcept("[FFMPEG] Error seeking frame to ", frameIndex, " in ", _filePath);
            return false;
        }
        
        // Need to flush the buffer manually
        avcodec_flush_buffers(codecContext);
    }

    while (received_frame < static_cast<int64_t>(frameIndex)) {
        int response = av_read_frame(formatContext, pkt);
        if(response < 0) {
            FormatExcept("[FFMPEG] Error reading frame ", frameCount, ": ", error_to_string(response));
            return false;
        }
        
        log_packet(pkt);
        
        if(pkt->stream_index != videoStreamIndex
           || avcodec_send_packet(codecContext, pkt) != 0)
        {
            av_packet_unref(pkt);
            continue;
        }
        
        av_packet_unref(pkt);

        while (true) {
            response = avcodec_receive_frame(codecContext, frame);
            
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                break; // Exit the loop to read the next packet
            } else if (response < 0) {
                FormatExcept("[FFMPEG] Error while receiving frame ", frameCount, " from decoder: ", error_to_string(response));
                return false;
            }
            
            received_frame = av_rescale_q(frame->pts, time_base, av_inv_q(frame_rate));
            
            if(received_frame == static_cast<int64_t>(frameIndex)) {
                /// This is exactly the frame that we wanted:
                frameCount = frameIndex;
                return true;
                
            } else if (received_frame > static_cast<int64_t>(frameIndex)) {
                FormatWarning("Here we land on a frame that is larger than we wanted: ", received_frame, " > ", static_cast<int64_t>(frameIndex));
                frameCount = received_frame;
                av_frame_unref(frame);
                return false;
            }

            av_frame_unref(frame);
        }
    }

    return false;
}

bool FfmpegVideoCapture::transfer_frame_to_software(AVFrame* frame) {
    if (!sw_frame) {
        sw_frame = av_frame_alloc();
        if (!sw_frame) return false;
    }

    if (av_hwframe_transfer_data(sw_frame, frame, 0) < 0) return false;

    av_frame_unref(frame);
    av_frame_move_ref(frame, sw_frame);
    return true;
}

template<typename Mat>
bool FfmpegVideoCapture::convert_frame_to_mat(const AVFrame* frame, Mat& mat) {
    uint cols, rows, dims;
    uint8_t* data;

    if constexpr(std::is_same_v<Mat, cv::Mat>
              || std::is_same_v<Mat, gpuMat>) 
    {
        cols = mat.cols;
		rows = mat.rows;
		dims = mat.channels();
	} else {
		cols = mat.cols;
		rows = mat.rows;
		dims = mat.dims;
	}

    if (!update_sws_context(frame, dims))
        return false;

    uint8_t channels = swsInputFormat == AV_PIX_FMT_RGBA
            ? 4
            : (swsInputFormat == AV_PIX_FMT_RGB24 ? 3 : 1);
    if (cols != (uint)frame->width
        || rows != (uint)frame->height
        || dims != channels)
    {
        if constexpr (std::is_same_v<Mat, Image>)
            mat.create(frame->height, frame->width, channels);
        else
            mat.create(frame->height, frame->width, CV_8UC(channels));

        dims = channels;
        Print("Created with: ", frame->height, " ", frame->width, " ", channels);
    }
    
    if constexpr(std::is_same_v<Mat, cv::Mat>
              || std::is_same_v<Mat, gpuMat>)
    {
        cols = mat.cols;
        rows = mat.rows;
        dims = mat.channels();
        if constexpr (std::is_same_v<Mat, cv::Mat>) {
            data = mat.data;
        }
    } else {
        cols = mat.cols;
        rows = mat.rows;
        dims = mat.dims;
        data = mat.data();
    }

    if constexpr (std::is_same_v<Mat, gpuMat>) {
        if (_buffer.dims != dims
            || _buffer.cols != static_cast<uint32_t>(frame->width)
            || _buffer.rows != static_cast<uint32_t>(frame->height))
        {
            _buffer.create(frame->height, frame->width, channels);
        }

        data = _buffer.data();
    }
    
    //cv::Mat tmp(frame->height, frame->width, CV_8UC3);
    uint8_t* dest[4] = { data };
    int dest_linesize[4] = { static_cast<int>(frame->width * channels * sizeof(uchar)) };
    sws_scale(swsContext, frame->data, frame->linesize, 0, frame->height, dest, dest_linesize);

    if constexpr (std::is_same_v<Mat, gpuMat>) {
        _buffer.get().copyTo(mat);
    }
    
#ifndef NDEBUG
    auto str = Meta::toStr(frameCount);
    if constexpr(std::is_same_v<Mat, Image>) {
        cv::putText(mat.get(), str, Vec2(100,100), cv::FONT_HERSHEY_PLAIN, 1, gui::White);
    } else {
        cv::putText(mat, str, Vec2(100,100), cv::FONT_HERSHEY_PLAIN, 1, gui::White);
    }
#endif
    return true;
}

bool FfmpegVideoCapture::update_sws_context(const AVFrame* frame, int dims) {
    auto inputformat = dims == 0 || dims == 4 ? AV_PIX_FMT_RGBA : (dims == 1 ? AV_PIX_FMT_GRAY8 : AV_PIX_FMT_RGB24);
    
    if (!swsContext
        || swsFormat != (AVPixelFormat)frame->format
        || swsInputFormat != inputformat)
    {
        if (swsContext)
            sws_freeContext(swsContext);

        swsFormat = (AVPixelFormat)frame->format;
        swsInputFormat = inputformat;
        swsContext = sws_getContext(frame->width, frame->height, swsFormat,
                                    frame->width, frame->height, swsInputFormat,
                                    SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (not swsContext)
            return false;

    }
    return true;
}

bool FfmpegVideoCapture::read(uint32_t frameIndex, cv::Mat& mat) {
    return FfmpegVideoCapture::_read<cv::Mat>(frameIndex, mat);
}

bool FfmpegVideoCapture::read(uint32_t frameIndex, cv::UMat& mat) {
    return FfmpegVideoCapture::_read<cv::UMat>(frameIndex, mat);
}

bool FfmpegVideoCapture::read(uint32_t frameIndex, Image& mat) {
    return FfmpegVideoCapture::_read<Image>(frameIndex, mat);
}

template<typename Mat>
bool FfmpegVideoCapture::decode_frame(Mat& outFrame) {
    if (frame->hw_frames_ctx
        && not transfer_frame_to_software(frame))
    {
        FormatError("Error transferring frame to software format.");
        return false;
    }
    
    if (not convert_frame_to_mat<Mat>(frame, outFrame)) {
        FormatError("Error converting frame to cv::Mat.");
        return false;
    }
    
    return true;
}

template<typename Mat>
bool FfmpegVideoCapture::_read(uint32_t frameIndex, Mat& outFrame) {
    if (!is_open())
        return false;

    static Timing timing("ffmpeg::read_frame");
    if(seek_frame(frameIndex)) {
        TakeTiming take(timing);
        
        if(frameCount == frameIndex) {
            /// we already received it!
            auto result = decode_frame(outFrame);
            av_frame_unref(frame);
            return result;
        }
        
#ifndef NDEBUG
        FormatExcept("Was unable to load ", frameIndex, " - instead have ", frameCount, ".");
#endif
        return false;
    }

    if constexpr (std::is_same_v<Mat, gpuMat>) {
        outFrame.setTo(0);
    }
    else if constexpr(are_the_same<Mat, Image>) {
        outFrame.set_to(0);
    }
    else {
        outFrame.setTo(0);
    }
    
    return false;
}

FfmpegVideoCapture::~FfmpegVideoCapture() {
    close();
}

void FfmpegVideoCapture::close() {
    _capture = nullptr;

    if (pkt) {
        av_packet_free(&pkt);
        pkt = nullptr;
    }
    if(sw_frame) {
        av_frame_free(&sw_frame);
        sw_frame = nullptr;
    }
    if (frame) {
        av_frame_free(&frame);
        frame = nullptr;
    }
    if(swsContext) {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }
    if (codecContext) {
        avcodec_free_context(&codecContext);
        codecContext = nullptr;
    }
    if (formatContext) {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }
    if(hw_device_ctx) {
        // Don't forget to free the device context when done
        av_buffer_unref(&hw_device_ctx);
        hw_device_ctx = nullptr;
    }
}

}
