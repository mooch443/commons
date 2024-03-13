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
    av_log_set_level(AV_LOG_QUIET);
}

bool FfmpegVideoCapture::open(const std::string& filePath) {
    //av_log_set_level(AV_LOG_DEBUG);
    // Allocate format context
    formatContext = avformat_alloc_context();
    if (not formatContext) {
        FormatError("Failed to allocate format context");
        return false;
    }

    //_capture = std::make_unique<cv::VideoCapture>(filePath);

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

    // Calculate duration in seconds
    AVStream *videoStream = formatContext->streams[videoStreamIndex];
    double durationInSeconds = videoStream->duration * av_q2d(videoStream->time_base);

    std::cout << "Duration: " << durationInSeconds << " seconds" << std::endl;
    std::cout << "nb_frames: " << videoStream->nb_frames << std::endl;
    std::cout << "av_q2d(videoStream->time_base): " << av_q2d(videoStream->time_base)<< " " << videoStream->time_base.num << "/" << videoStream->time_base.den << std::endl;
    
#ifndef NDEBUG
    AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_NONE;
    while ((hw_type = av_hwdevice_iterate_types(hw_type)) != AV_HWDEVICE_TYPE_NONE) {
        print("HW type: ", hw_type);
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
        }
    }

    hw_start_index = i;

    if (hw_device_ctx == nullptr) {
        FormatWarning("Failed to create a hardware device context: ", preferred_devices);
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
        FormatError("Unsupported codec id: ", codecParameters->codec_id);
        return false;
    }

    // ffmpeg -y -vsync 0 -hwaccel cuda -hwaccel_output_format cuda -i input.mp4 -c:a copy -c:v h264_nvenc -preset p6 -tune ll -b:v 5M -bufsize 5M -maxrate 10M -qmin 0 -g 250 -bf 3 -b_ref_mode middle -temporal-aq 1 -rc-lookahead 20 -i_qfactor 0.75 -b_qfactor 1.1 output.mp4
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
        avcodec_free_context(&codecContext);

        codecContext = nullptr;
        codec = nullptr;
        if(sign_cast<size_t>(hw_start_index) <= preferred_devices.size()) {
			FormatWarning("Failed to open codec with hardware acceleration. Retrying with next hardware device.");
			goto retry_codec;
		}

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
       && (_disable_jumping
           || frameIndex < frameCount + int64_t(frame_rate.num / frame_rate.den) + 1))
    {
#ifndef NDEBUG
        print("jump seeking might be good seeking from ", frameCount, " to ", frameIndex,": ", frame_rate.num / frame_rate.den,"fps.");
        Timer timer;
        size_t i = 0;
#endif
        while(grab() && frameIndex > frameCount) {
#ifndef NDEBUG
            ++i;
#endif
        }
        
#ifndef NDEBUG
        auto e = timer.elapsed();
        if(i > 0 && e > 0)
            print("jumped grabbing ", i, " frames in ", e * 1000, "ms => ", double(i) / double(e),"fps");
#endif
        return frameIndex == frameCount;
    }

    // Convert frame index to a timestamp
    Timer timer;
    int64_t timestamp = -1;

    // find closest keyframe _before_ frameIndex from _keyframes:
    auto it = _keyframes.lower_bound(frameIndex - 1);
    if (it != _keyframes.begin()) {
        if (it == _keyframes.end() || it->first != frameIndex - 1) {
            // If the lower_bound did not find an exact match, decrement the iterator
            --it;
        }
        // 'it' now points to the closest frame that is less than or equal to the given frame
        auto keyframe = it->first;
        if (keyframe < frameIndex
            && abs(keyframe - frameIndex) < offset) 
        {            
#ifndef NDEBUG
            print("jump seeking from ", frameCount, " to ",frameIndex," (known keyframe=", keyframe, "): ", frame_rate.num / frame_rate.den, "fps.");
#endif
            timestamp = it->second;
        } 
#ifndef NDEBUG
        else
            print("NOT jump seeking from ", frameCount, " to ", frameIndex," (keyframe=", keyframe, "): ", offset, " > ", abs(keyframe - frameIndex), " to ", frameIndex);
#endif
    }

    if(timestamp == -1)
        timestamp = av_rescale_q(max(0, int64_t(frameIndex) - offset), src_tb, dst_tb);    

#ifndef NDEBUG
    print("jumping to timestamp = ", timestamp, " for frameindex ",frameIndex, " frame_rate=",frame_rate.num,"/", frame_rate.den, " dst.num=", dst_tb.num, " dst.den=", dst_tb.den, " gop=", codecContext->gop_size, " with offset=", offset);
    av_log_set_level(AV_LOG_DEBUG);
#endif
    // Seek directly to the calculated timestamp
    if (av_seek_frame(formatContext, videoStreamIndex, timestamp, AVSEEK_FLAG_FRAME) < 0) {
        FormatExcept("Error seeking frame to ", frameIndex, " in ", _filePath);
        return false;
    }
    
    avcodec_flush_buffers(codecContext);

    AVFrame* temp_frame = av_frame_alloc();
    if (!temp_frame) {
        FormatExcept("Failed to allocate temporary frame for seeking.");
        return false;
    }

    int64_t received_frame = -1;
#ifndef NDEBUG
    int64_t count_jumped_frames = 0;
#endif
    int tries = 0, frame_skips = 1;
    while (received_frame < static_cast<int64_t>(frameIndex) - 1) {
        int ret = av_read_frame(formatContext, pkt);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            FormatExcept("Error reading frame ",frameCount," during seeking: ", errbuf);
            if(tries++ > 1) {
                av_frame_free(&temp_frame);
                return false;
            } else
                continue;
        }

        if (pkt->stream_index == videoStreamIndex) {
            int response = avcodec_send_packet(codecContext, pkt);
            av_packet_unref(pkt); // Unref packet immediately after sending
            if (response < 0) {
                FormatError("Error sending frame packet");
                break;
            }
            
            av_frame_make_writable(temp_frame);
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
                                               AVRational{frame_rate.den, frame_rate.num});
#ifndef NDEBUG
            if(frame_index > frameIndex) {
                count_jumped_frames += frame_index - frameIndex + 1;
            } else {
                count_jumped_frames += frameIndex - frame_index + 1;
            }
#endif

#ifdef AV_FRAME_FLAG_KEY
            if(temp_frame->flags & AV_FRAME_FLAG_KEY)
#else
            if(temp_frame->key_frame == 1)
#endif
                _keyframes[frame_index] = temp_frame->pts;
            
            received_frame = frame_index;
            if (frame_index == static_cast<int>(max(1u, frameIndex) - 1)) {  
#ifndef NDEBUG
                print("found jumped to frame ", frame_index, " / ", frameIndex);
#endif
                break;
            } else if(frame_index >= frameIndex && timestamp == 0) {                
#ifndef NDEBUG
                print("jumped to ",frame_index," when trying to get ", frameIndex, " but timestamp == 0");
#endif
                return false;
                
            } else if(frame_index >= frameIndex) {
                // jumped ahead of frameIndex target...
                // we need to go back further.
#ifndef NDEBUG
                auto old_offset = offset;
#endif
                offset = int64_t(offset * 1.5);
                timestamp = av_rescale_q(max(0, int64_t(frameIndex) - offset), src_tb, dst_tb);  
#ifndef NDEBUG
                print("jumping back further (got:",frame_index," for offset=",old_offset,"): offset=", offset, " timestamp=", timestamp, "  for frame=", frameIndex);
#endif
                
                frame_skips++;
                
                // Seek directly to the calculated timestamp
                if (av_seek_frame(formatContext, videoStreamIndex, timestamp, AVSEEK_FLAG_FRAME) < 0) {
                    FormatExcept("Error seeking frame to ", frameIndex, " in ", _filePath);
                    return false;
                }
                received_frame = int64_t(frameIndex) - offset;
                avcodec_flush_buffers(codecContext);
                
            } else {
                if(frame_index < frameCount && frame_index < frameIndex
                   && frame_skips > 1)
                {
                    // we made it _worse_ by jumping. we should not
                    // keep doing this if the frame is below current:
                    _disable_jumping = true;
                    av_frame_free(&temp_frame);
                    
                    frameCount = frame_index;
                    while(grab() && frameIndex > frameCount) {
#ifndef NDEBUG
                        print("grabbing ", frameIndex," / ", frameCount);
#endif
                    }
                    
                    return frameCount == frameIndex;
                }
#ifndef NDEBUG
                print("jumped to frame ", frame_index, " / ", frameIndex);
#endif
                continue;
            }
            
        } else
            av_packet_unref(pkt);
    }

    av_frame_free(&temp_frame);

    // Update internal frame count
    frameCount = frameIndex;
    
#ifndef NDEBUG
    auto e = timer.elapsed();
    if(e > 0 && count_jumped_frames > 0)
        print("jump skipped ", count_jumped_frames, " frames in ", e * 1000, "ms => ", double(count_jumped_frames) / e,"fps");
#endif

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
        print("Created with: ", frame->height, " ", frame->width, " ", channels);
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
    //auto str = Meta::toStr(frameCount);
    //cv::putText(outFrame.get(), str, Vec2(100,100), cv::FONT_HERSHEY_PLAIN, 1, gui::White);
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

    /*_capture->grab();
    ++frameCount;
    return true;*/

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

#ifdef AV_FRAME_FLAG_KEY
        if (frame->flags & AV_FRAME_FLAG_KEY) {
#else
        if (frame->key_frame == 1) {
#endif
            _keyframes[frameCount] = frame->pts;
#ifndef NDEBUG
            print("keyframe ", frameCount, " is ", frame->pts);
#endif
        }

        ++frameCount;
        av_frame_unref(frame); // Unref frame after processing
        return true;
    }

    return false;
}

bool FfmpegVideoCapture::read(cv::Mat& mat) {
    return FfmpegVideoCapture::_read<cv::Mat>(mat);
}

bool FfmpegVideoCapture::read(cv::UMat& mat) {
    return FfmpegVideoCapture::_read<cv::UMat>(mat);
}

bool FfmpegVideoCapture::read(Image& mat) {
    return FfmpegVideoCapture::_read<Image>(mat);
}

template<typename Mat>
bool FfmpegVideoCapture::_read(Mat& outFrame) {
    if (!is_open()) return false;

    /*uint8_t channels = swsInputFormat == AV_PIX_FMT_NONE || swsInputFormat == AV_PIX_FMT_RGBA
        ? 4
        : (swsInputFormat == AV_PIX_FMT_RGB24 ? 3 : 1);
    if (is_in(outFrame.dims, 1, 3, 4))
        channels = outFrame.dims;

    if (outFrame.cols != (uint)dimensions().width
        || outFrame.rows != (uint)dimensions().height
        || outFrame.dims != channels)
    {
        outFrame.create(dimensions().height, dimensions().width, channels);
        print("Created with: ", frame->height, " ", frame->width, " ", channels);
    }

    cv::Mat mat;
    _capture->read(mat);
    ++frameCount;

    if (mat.channels() == channels)
        mat.copyTo(outFrame.get());
    else if(mat.channels() == 3 && channels == 4)
        cv::cvtColor(mat, outFrame.get(), cv::COLOR_BGR2BGRA);
    else if(mat.channels() == 4 && channels == 3)
        cv::cvtColor(mat, outFrame.get(), cv::COLOR_BGRA2BGR);
    else if(mat.channels() == 1 && channels == 3)
        cv::cvtColor(mat, outFrame.get(), cv::COLOR_GRAY2BGR);
    else if(mat.channels() == 1 && channels == 4)
        cv::cvtColor(mat, outFrame.get(), cv::COLOR_GRAY2BGRA);
    else
        return false;

    return true;*/

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

        }
        else if (response < 0) {
            FormatError("Error while receiving frame from decoder");
            break;
        }

#ifdef AV_FRAME_FLAG_KEY
        int64_t timestamp = frame->flags & AV_FRAME_FLAG_KEY ? frame->pts : -1;
#else
        int64_t timestamp = frame->key_frame == 1 ? frame->pts : -1;
#endif

/*
        if constexpr (std::is_same_v<Mat, gpuMat>) {
            if (frame->hw_frames_ctx) {
                if (cv::hw_copy_frame_to_umat(frame->hw_frames_ctx, frame, outFrame)) {
                    ++frameCount;
                    av_frame_unref(frame); // Unref frame after processing
                    return true;
                }
            }
        }
*/
        if (frame->hw_frames_ctx
            && not transfer_frame_to_software(frame))
        {
            FormatError("Error transferring frame to software format.");
            break;
        }

        if (not convert_frame_to_mat<Mat>(frame, outFrame)) {
            FormatError("Error converting frame to cv::Mat.");
            break;
        }

        if (timestamp >= 0) {
            _keyframes[frameCount] = timestamp;
#ifndef NDEBUG
            print("_read: keyframe ", frameCount, " is ", timestamp);
#endif
        }

        ++frameCount;
        av_frame_unref(frame); // Unref frame after processing
        return true;
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
