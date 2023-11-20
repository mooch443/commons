#include "FFmpegVideoCapture.h"
#include <misc/format.h>
#include <misc/Timer.h>

namespace cmn {

FfmpegVideoCapture::FfmpegVideoCapture(const std::string& filePath) {
    avformat_network_init();
    if (not filePath.empty()
        && not open(filePath)) {
        // Handle error if opening the file fails
        std::cerr << "Failed to open video file: " << filePath << std::endl;
    }
}

bool FfmpegVideoCapture::open(const std::string& filePath) {
    // Allocate format context
    formatContext = avformat_alloc_context();
    if (not formatContext) {
        FormatError("Failed to allocate format context");
        return false;
    }

    // Open video file
    if (avformat_open_input(&formatContext, filePath.c_str(), nullptr, nullptr) != 0) {
        FormatError("Failed to open video file: ", filePath);
        return false;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        FormatError("Failed to find stream information");
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
        FormatError("Failed to find video stream");
        return false;
    }

    AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_NONE;
    while ((hw_type = av_hwdevice_iterate_types(hw_type)) != AV_HWDEVICE_TYPE_NONE) {
        // Check if this hardware type is supported by the codec
        // You can also apply your own prioritization logic here
        if (av_hwdevice_ctx_create(&hw_device_ctx, hw_type, nullptr, nullptr, 0) < 0) {
            FormatWarning("Failed to create a hardware device context: ", hw_type);
        } else
            break;
    }
    
    // Get a pointer to the codec for the video stream
    AVCodecParameters* codecParameters = formatContext->streams[videoStreamIndex]->codecpar;
    const AVCodec* codec{nullptr};
    codec = avcodec_find_decoder(codecParameters->codec_id);
    if (!codec) {
        FormatError("Unsupported codec id: ", codecParameters->codec_id);
        return false;
    }

    // Allocate codec context
    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        FormatError("Failed to allocate codec context");
        return false;
    }

    // Copy codec parameters to codec context
    if (avcodec_parameters_to_context(codecContext, codecParameters) < 0) {
        FormatError("Failed to copy codec parameters to codec context");
        return false;
    }
    
    if(hw_device_ctx) {
        // Step 3: Assign the hardware context to the codec context
        codecContext->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    }
    
    // Open codec
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        FormatError("Failed to open codec");
        return false;
    }
    
    // Allocate frame and packet
    frame = av_frame_alloc();
    pkt = av_packet_alloc();
    if (!frame || !pkt) {
        FormatError("Failed to allocate frame or packet");
        return false;
    }

    _filePath = filePath;
    return true;
}

bool FfmpegVideoCapture::is_open() const {
    return formatContext != nullptr;
}

int64_t FfmpegVideoCapture::length() const {
    if (!is_open()) return -1;
    return formatContext->streams[videoStreamIndex]->nb_frames;
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
    return codecContext->framerate.num / codecContext->framerate.den;
}

int FfmpegVideoCapture::channels() const {
    // Assuming video frames are in a standard format, they have 3 channels (RGB or YUV)
    return 4;
}

bool FfmpegVideoCapture::set_frame(uint32_t index) {
    if (!is_open()) return false;
    return seek_frame(index);
}

bool FfmpegVideoCapture::seek_frame(uint32_t frameIndex) {
    // Convert frame index to a timestamp
    if(frameCount == frameIndex)
        return true;
    
    AVRational time_base = formatContext->streams[videoStreamIndex]->time_base;
    AVRational frame_rate = formatContext->streams[videoStreamIndex]->r_frame_rate;

    // Source time base: each unit is one frame
    AVRational src_tb = {frame_rate.den, frame_rate.num};

    // Destination time base: stream's time base
    AVRational dst_tb = time_base;
    
    //int64_t offset = max(1, int64_t(codecContext->gop_size) + 1);
    int64_t offset = max(1, frameCount < frameIndex
                                 ? int64_t(frame_rate.num / frame_rate.den / 2)
                                 : int64_t(frame_rate.num / frame_rate.den)) + 1;
    
    if(frameIndex > frameCount
       && frameIndex < frameCount + int64_t(frame_rate.num / frame_rate.den) + 1)
    {
        print("jump seeking might be good seeking from ", frameCount, " to ", frameIndex,": ", frame_rate.num / frame_rate.den,"fps.");
        Timer timer;
        size_t i = 0;
        while(grab() && frameIndex > frameCount) { ++i; }
        auto e = timer.elapsed();
        if(i > 0 && e > 0)
            print("jumped grabbing ", i, " frames in ", e * 1000, "ms => ", double(i) / double(e),"fps");
        return frameIndex == frameCount;
    }

    // Convert frame index to a timestamp
    auto initial_frame = frameCount;
    Timer timer;
    int64_t timestamp = av_rescale_q(max(0, int64_t(frameIndex) - offset), src_tb, dst_tb);
    print("jumping to timestamp = ", timestamp, " for frameindex ",frameIndex, " frame_rate=",frame_rate.num,"/", frame_rate.den, " dst.num=", dst_tb.num, " dst.den=", dst_tb.den, " gop=", codecContext->gop_size, " with offset=", offset);
    
    av_log_set_level(AV_LOG_DEBUG);
    // Seek directly to the calculated timestamp
    if (av_seek_frame(formatContext, videoStreamIndex, timestamp, AVSEEK_FLAG_ANY) < 0) {
        FormatExcept("Error seeking frame to ", frameIndex, " in ", _filePath);
        return false;
    }
    
    avcodec_flush_buffers(codecContext);

    AVFrame* temp_frame = av_frame_alloc();
    if (!temp_frame) {
        FormatExcept("Failed to allocate temporary frame for seeking.");
        return false;
    }
    //av_frame_make_writable(temp_frame);

    int received_frame = -1;
    int64_t count_jumped_frames = 0;
    while (received_frame < static_cast<int>(frameIndex - 1)) {
        if (av_read_frame(formatContext, pkt) < 0) {
            FormatExcept("Error reading frame during seeking.");
            av_frame_free(&temp_frame);
            return false;
        }

        if (pkt->stream_index == videoStreamIndex) {
            int response = avcodec_send_packet(codecContext, pkt);
            av_packet_unref(pkt); // Unref packet immediately after sending
            if (response < 0) {
                FormatError("Error sending packet");
                break;
            }

            response = avcodec_receive_frame(codecContext, temp_frame);
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                continue;
                
            } else if (response < 0) {
                FormatError("Error while receiving frame from decoder");
                return false;
            }
            
            // Convert the pts to a frame index
            int64_t frame_index = av_rescale_q(temp_frame->pts,
                                               time_base,
                                               AVRational{1, frame_rate.num});
            if(frame_index > frameIndex) {
                count_jumped_frames += frame_index - frameIndex + 1;
            } else {
                count_jumped_frames += frameIndex - frame_index + 1;
            }
            
            received_frame = frame_index;
            if (frame_index == static_cast<int>(max(1u, frameIndex) - 1)) {
                print("found jumped to frame ", frame_index, " / ", frameIndex);
                break;
            } else if(frame_index >= frameIndex && timestamp == 0) {
                print("jumpted to ",frame_index," when trying to get ", frameIndex, " but timestamp == 0");
                return false;
                
            } else if(frame_index >= frameIndex) {
                auto old_offset = offset;
                offset = int64_t(offset * 1.5);
                timestamp = av_rescale_q(max(0, int64_t(frameIndex) - offset), src_tb, dst_tb);
                print("jumping back further (got:",frame_index," for offset=",old_offset,"): offset=", offset, " timestamp=", timestamp, "  for frame=", frameIndex);
                
                // Seek directly to the calculated timestamp
                if (av_seek_frame(formatContext, videoStreamIndex, timestamp, AVSEEK_FLAG_ANY) < 0) {
                    FormatExcept("Error seeking frame to ", frameIndex, " in ", _filePath);
                    return false;
                }
                received_frame = int64_t(frameIndex) - offset;
                avcodec_flush_buffers(codecContext);
                
            } else {
                print("jumped to frame ", frame_index, " / ", frameIndex);
                continue;
            }
            
        } else
            av_packet_unref(pkt);
    }

    av_frame_free(&temp_frame);

    // Update internal frame count
    frameCount = frameIndex;
    auto e = timer.elapsed();
    if(e > 0 && count_jumped_frames > 0)
        print("jump skipped ", count_jumped_frames, " frames in ", e * 1000, "ms => ", double(count_jumped_frames) / e,"fps");

    return true;
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

bool FfmpegVideoCapture::convert_frame_to_mat(const AVFrame* frame, Image& outFrame) {
    if (!update_sws_context(frame, outFrame.dims))
        return false;

    uint8_t channels = swsInputFormat == AV_PIX_FMT_RGBA
            ? 4
            : (swsInputFormat == AV_PIX_FMT_RGB24 ? 3 : 1);
    if (outFrame.cols != (uint)frame->width
        || outFrame.rows != (uint)frame->height
        || outFrame.dims != channels)
    {
        outFrame.create(frame->height, frame->width, channels);
        print("Created with: ", frame->height, " ", frame->width, " ", channels);
    }
    
    //cv::Mat tmp(frame->height, frame->width, CV_8UC3);
    uint8_t* dest[4] = { outFrame.data() };
    int dest_linesize[4] = { static_cast<int>(outFrame.cols * outFrame.dims * sizeof(uchar)) };
    sws_scale(swsContext, frame->data, frame->linesize, 0, frame->height, dest, dest_linesize);

    auto str = Meta::toStr(frameCount);
    cv::putText(outFrame.get(), str, Vec2(100,100), cv::FONT_HERSHEY_PLAIN, 1, gui::White);
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

bool FfmpegVideoCapture::grab() {
    if (!is_open()) return false;

    while (av_read_frame(formatContext, pkt) >= 0) {
        if (pkt->stream_index != videoStreamIndex) {
            av_packet_unref(pkt);
            continue;
        }

        int response = avcodec_send_packet(codecContext, pkt);
        av_packet_unref(pkt); // Unref packet immediately after sending
        if (response < 0)
            break;

        response = avcodec_receive_frame(codecContext, frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            continue;
            
        } else if (response < 0) {
            FormatError("Error while receiving frame from decoder");
            break;
        }

        ++frameCount;
        av_frame_unref(frame); // Unref frame after processing
        return true;
    }

    return false;
}

bool FfmpegVideoCapture::read(Image& outFrame) {
    if (!is_open()) return false;

    int response;
    while (av_read_frame(formatContext, pkt) >= 0) {
        if (pkt->stream_index != videoStreamIndex) {
            av_packet_unref(pkt);
            continue;
        }

        response = avcodec_send_packet(codecContext, pkt);
        av_packet_unref(pkt); // Unref packet immediately after sending
        if (response < 0)
            break;

        response = avcodec_receive_frame(codecContext, frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            continue;
            
        } else if (response < 0) {
            FormatError("Error while receiving frame from decoder");
            break;
        }

        if (frame->hw_frames_ctx
            && not transfer_frame_to_software(frame))
        {
            FormatError("Error transferring frame to software format.");
            return false;
        }

        if (not convert_frame_to_mat(frame, outFrame)) {
            FormatError("Error converting frame to cv::Mat.");
            return false;
        }

        ++frameCount;
        av_frame_unref(frame); // Unref frame after processing
        return true;
    }

    return false;
}

FfmpegVideoCapture::~FfmpegVideoCapture() {
    close();
}

void FfmpegVideoCapture::close() {
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
