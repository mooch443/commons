#include "FFmpegVideoCapture.h"
#include <misc/Timer.h>

//#undef NDEBUG
//#define DEBUG_FFMPEG_PACKETS
//#define DEBUG_FFMPEG_FRAMES

namespace cmn {

IMPLEMENT(FfmpegVideoCapture::tested_video_lengths);
IMPLEMENT(FfmpegVideoCapture::tested_video_lengths_mutex);

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

    //std::cout << "Duration: " << durationInSeconds << " seconds" << std::endl;
    //std::cout << "L: " << codecContext->framerate.num / codecContext->framerate.den << " frames" << std::endl;
    auto fr = static_cast<double>(double(formatContext->streams[ videoStreamIndex ]->avg_frame_rate.num) / formatContext->streams[ videoStreamIndex ]->avg_frame_rate.den);
    //std::cout << " => " << durationInSeconds * fr << " frames" << std::endl;
    
    //std::cout << "nb_frames: " << videoStream->nb_frames << std::endl;
    //std::cout << "av_q2d(videoStream->time_base): " << av_q2d(videoStream->time_base)<< " " << videoStream->time_base.num << "/" << videoStream->time_base.den << std::endl;
    
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
    
    if(not filePath.empty()) {
        auto results = get_actual_frame_count(*this, filePath);
        actual_frame_count_ = results.length;
        _is_greyscale = results.is_greyscale;
    } else {
        actual_frame_count_ = -1;
        _is_greyscale = false;
    }
    
    return true;
}

bool FfmpegVideoCapture::is_open() const {
    return formatContext != nullptr;
}

bool FfmpegVideoCapture::is_greyscale() const {
    return _is_greyscale;
}

void FfmpegVideoCapture::recovered_error(const std::string_view& str) const {
    if(not _recovered_errors.contains(str))
        _recovered_errors.insert(str);
}

int64_t FfmpegVideoCapture::estimated_frame_count() const {
    // Attempt to get the number of frames from nb_frames
    int64_t N = formatContext->streams[videoStreamIndex]->nb_frames;
    int64_t decoder_delay = codecContext->has_b_frames;
    N -= decoder_delay + 1;
    
    if (N > 0) {
        return N;
    }

    // Fallback to estimate based on duration and frame rate
    double durationInSeconds = formatContext->duration / static_cast<double>(AV_TIME_BASE);
    int fr = frame_rate();
    if (fr <= 0) {
        return -1;
    }
    return static_cast<int64_t>(durationInSeconds * fr + 0.5); // Rounded to nearest integer
}

FfmpegVideoCapture::VideoTestResults FfmpegVideoCapture::get_actual_frame_count(FfmpegVideoCapture&cap, const file::Path &path) {
    std::unique_lock guard{tested_video_lengths_mutex};
    if (auto it = tested_video_lengths.find(path.str());
        it != tested_video_lengths.end())
    {
        return it->second;
    }
    
    auto estimate = cap.estimated_frame_count();
    auto results = calculate_actual_frame_count(cap, path);
    if(estimate != results.length)
        FormatWarning(" * ", path, " actual frame count = ", results.length, " (instead of ", estimate,"). remembering this.");
    else
        Print(" * ", path, " is giving true length information (",results.length," frames).");
    tested_video_lengths[path.str()] = results;
    return results;
}

FfmpegVideoCapture::VideoTestResults FfmpegVideoCapture::calculate_actual_frame_count(FfmpegVideoCapture&cap, const file::Path& ) {
    // Check if we have already calculated the frame count
    VideoTestResults results;
    
    //FfmpegVideoCapture cap{path.str()};
    //if(not cap.open(path.str())) return -1;
    if (!cap.is_open()) return {};

    int64_t estimated_count = cap.estimated_frame_count(); // Use nb_frames or duration * frame_rate

    // Step 1: Try to seek to the estimated end frame
    if (cap.seek_frame(static_cast<uint32_t>(estimated_count))) {
        // Estimated frame exists, assume estimated_count is correct
        results.length = estimated_count + 1;
    } else {
        // Step 2: Go back a few frames and test from there
        int64_t frame_rate = cap.frame_rate();
        if (frame_rate <= 0) {
            frame_rate = 25; // Default to 25 fps if frame rate is invalid
        }

        // Determine how many frames to step back each time
        int64_t step = frame_rate * 1; // Adjust as needed

        int64_t last_valid_frame = -1;
        int64_t current_index = max(0, estimated_count - 1 - step);
        bool found = false;

        while (current_index < estimated_count) {
            //Print("* testing frame ", current_index);
            if (not cap.seek_frame(static_cast<uint32_t>(current_index))) {
                break;
            }
            last_valid_frame = current_index;
            ++current_index;
        }
        
        if(last_valid_frame != -1) {
            results.length = last_valid_frame;
            found = true;
        }

        // If found, return the actual frame count
        if (found) {
            /// nothing
        } else {
            // Step 3: Perform binary search to find the actual frame count
            int64_t low = 0;
            int64_t high = current_index + step; // Start from the last tested index
            int64_t mid;
            int64_t last_valid_frame = -1;

            while (low <= high) {
                mid = (low + high) / 2;

                if (cap.seek_frame(static_cast<uint32_t>(mid))) {
                    // Frame exists, move to higher indices
                    low = mid + 1;
                    last_valid_frame = mid;
                } else {
                    // Frame does not exist, move to lower indices
                    high = mid - 1;
                }
            }

            // The total number of frames is last_valid_frame
            results.length = last_valid_frame + 1;
        }
    }
    
    // --- New code for dynamic greyscale detection ---
    {
        bool isDynamicGreyscale = false;
        // Seek to the beginning of the video (frame index 0)
        cv::Mat testFrame = cv::Mat::zeros(0, 0, CV_8UC(cap.channels()));
        // Use the overloaded read function to decode frame 0 into a cv::Mat
        if (cap.read(0, testFrame)) {
            // If the frame has only one channel, it is grayscale.
            if (testFrame.channels() == 1) {
                isDynamicGreyscale = true;
            }
            // For multi-channel images, verify that all channels are identical.
            else if (testFrame.channels() > 1) {
                std::vector<cv::Mat> channels;
                cv::split(testFrame, channels);
                cv::Mat diff;
                cv::absdiff(channels[0], channels[1], diff);
                if (cv::countNonZero(diff) == 0) {
                    cv::absdiff(channels[1], channels[2], diff);
                    if (cv::countNonZero(diff) == 0)
                        isDynamicGreyscale = true;
                }
            } else {
                FormatExcept("Cannot determine greyscale of video with ", testFrame.channels(), " channels.");
            }
        }
        
        results.is_greyscale = isDynamicGreyscale;
    }
    // --- End of dynamic greyscale detection ---
    
    return results;
}

int64_t FfmpegVideoCapture::length() const {
    if (!is_open()) return -1;
    
    if(actual_frame_count_ >= 0) {
        return actual_frame_count_;
    }
    
    AVStream *videoStream = videoStreamIndex < int64_t(formatContext->nb_streams)
        ? formatContext->streams[videoStreamIndex]
        : nullptr;
    if(not videoStream)
        return 0;
    
    int64_t N = formatContext->streams[videoStreamIndex]->nb_frames;
    int64_t decoder_delay = codecContext->has_b_frames;
    int64_t _decoder_delay = codecContext->delay;
#ifndef NDEBUG
    Print("* Stream reports decoder_delay of ", decoder_delay, " vs ", _decoder_delay, " frames. Subtracting from L=",N);
#endif
    N -= decoder_delay;
    
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
    if(codecContext->framerate.num <= 0
        || codecContext->framerate.den <= 0
        || codecContext->framerate.num / codecContext->framerate.den >= 1000) {
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
    switch (swsInputFormat) {
        case AV_PIX_FMT_RGBA: return 4;
        case AV_PIX_FMT_RGB24: return 3;
        case AV_PIX_FMT_GRAY8: return 1;
        case AV_PIX_FMT_YUV420P: return 3;  // YUV has 3 planes
        case AV_PIX_FMT_YUV422P: return 3;
        case AV_PIX_FMT_YUV444P: return 3;
        case AV_PIX_FMT_NV12: return 2;  // NV12 packs UV together
        default: return 1;  // Assume monochrome if unknown
    }
}

void FfmpegVideoCapture::log_packet([[maybe_unused]] const AVPacket * pkt) {
#if !defined(NDEBUG) && !defined(__linux__) && defined(DEBUG_FFMPEG_PACKETS)
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

    // Log the initial state
#ifdef DEBUG_FFMPEG_FRAMES
    Print("[seek_frame] Attempting to seek to frame ", frameIndex);
#endif

    AVRational time_base = formatContext->streams[videoStreamIndex]->time_base;
    AVRational frame_rate = av_guess_frame_rate(formatContext, formatContext->streams[videoStreamIndex], nullptr);

    // Calculate the keyframe interval
    int keyframe_interval = formatContext->streams[videoStreamIndex]->avg_frame_rate.num / formatContext->streams[videoStreamIndex]->avg_frame_rate.den;

    int64_t received_frame = -1;

    // Determine if we should seek or decode sequentially
    if (current_frame < static_cast<int64_t>(frameIndex)
        && abs(static_cast<int64_t>(frameIndex) - current_frame) <= keyframe_interval)
    {
        received_frame = current_frame;

#ifdef DEBUG_FFMPEG_FRAMES
        Print("[seek_frame] Decoding sequentially from current frame ", current_frame, " to ", frameIndex);
#endif

    } else {
        // If far away, seek directly to the calculated timestamp
        int64_t timestamp = av_rescale_q(max(0, static_cast<int64_t>(frameIndex)), av_inv_q(frame_rate), time_base);

#ifdef DEBUG_FFMPEG_FRAMES
        Print("[seek_frame] Seeking to timestamp ", timestamp, " for frame ", frameIndex);
#endif

        if (av_seek_frame(formatContext, videoStreamIndex, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
            FormatExcept("[FFMPEG] Error seeking frame to ", frameIndex, " in ", _filePath);
            return false;
        }

        // Need to flush the buffer manually
        avcodec_flush_buffers(codecContext);

        // we are actively setting a time, cannot assume sequential
        last_seq_received_frame.reset();
        
#ifdef DEBUG_FFMPEG_FRAMES
        Print("[seek_frame] Flushed codec buffers after seeking.");
#endif
    }

    while (received_frame < static_cast<int64_t>(frameIndex)) {
        int response = av_read_frame(formatContext, pkt);
        if (response < 0) {
            FormatExcept("[FFMPEG] Error reading frame ", received_frame + 1, ": ", error_to_string(response));
            return false;
        }

        log_packet(pkt);

        if (pkt->stream_index != videoStreamIndex
            || avcodec_send_packet(codecContext, pkt) != 0)
        {
            av_packet_unref(pkt);
            continue;
        }

        av_packet_unref(pkt);

        while (true) {
            response = avcodec_receive_frame(codecContext, frame);

            if (response == AVERROR(EAGAIN)) {
                break; // Exit the loop to read the next packet
            } else if (response == AVERROR_EOF) {
                break; // EOF
            } else if (response < 0) {
                FormatExcept("[FFMPEG] Error while receiving frame ", current_frame, " from decoder: ", error_to_string(response));
                return false;
            }
            
            /*{
                int64_t timestamp = frame->best_effort_timestamp;
                double frame_index = double(timestamp) * double(time_base.num) / double(time_base.den) * double(frame_rate.den) / double(frame_rate.num);
                auto actual = av_rescale_q(timestamp, time_base, av_inv_q(frame_rate));
                
                double fractional_part = actual - floor(frame_index);
                
                if (fractional_part > 0.001) { // Threshold for rounding error
                    FormatWarning("Rounding error detected in frame index calculation. Fractional part: ", fractional_part);
                }
                
                //auto rounded = static_cast<int64_t>(round(frame_index));
                //FormatWarning("rounded frame_index = ", rounded, " original: ", actual);
            }*/
            
            auto previous_frame = received_frame;
            received_frame = av_rescale_q(frame->best_effort_timestamp, time_base, av_inv_q(frame_rate));
            
            // we have received a frame, so lets save it
            if (last_seq_received_frame) {
                int64_t expected_frame = previous_frame + 1; // Assuming sequential read
                if (received_frame > expected_frame) {
                    recovered_error("Dropped frames during video decoding.");
                    FormatWarning("Detected dropped frames. Expected frame: ", expected_frame, ", but received: ", received_frame);
                }
            }

            last_seq_received_frame = received_frame;
            
#ifdef DEBUG_FFMPEG_FRAMES
            Print("[seek_frame] Received frame ", received_frame);
#endif

            if (received_frame == static_cast<int64_t>(frameIndex)) {
                // This is exactly the frame that we wanted
                current_frame = frameIndex;

#ifdef DEBUG_FFMPEG_FRAMES
                Print("[seek_frame] Successfully reached frame ", frameIndex);
#endif

                return true;

            } else if (received_frame > static_cast<int64_t>(frameIndex)) {
                FormatWarning("Here we land on a frame that is larger than we wanted: ", received_frame, " > ", static_cast<int64_t>(frameIndex));
                current_frame = received_frame;
                av_frame_unref(frame);

#ifdef DEBUG_FFMPEG_FRAMES
                Print("[seek_frame] Overshot the desired frame. Current frame: ", received_frame);
#endif

                return false;
            }

            av_frame_unref(frame);
        }
    }

#ifdef DEBUG_FFMPEG_FRAMES
    Print("[seek_frame] Could not reach frame ", frameIndex);
#endif

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
#ifndef NDEBUG
        Print("Created with: ", frame->height, " ", frame->width, " ", channels);
#endif
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
    auto str = Meta::toStr(current_frame);
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
    if(frameIndex >= length()) {
#ifndef NDEBUG
        FormatExcept("Unable to load frame ", frameIndex, " from file of length ", length());
#endif
        return false;
    }

    static Timing timing("ffmpeg::read_frame");
    if(seek_frame(frameIndex)) {
        TakeTiming take(timing);
        
        if(current_frame == frameIndex) {
            /// we already received it!
            auto result = decode_frame(outFrame);
            av_frame_unref(frame);
            return result;
        }
        
#ifndef NDEBUG
        FormatExcept("Was unable to load ", frameIndex, " - instead have ", current_frame, ".");
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
