#include "AveragingAccumulator.h"
#include <misc/GlobalSettings.h>
#include <misc/Image.h>
#include <misc/SpriteMap.h>
#include <misc/Timer.h>

namespace cmn {

ENUM_CLASS_DOCS(averaging_method_t,
    "Sum all samples and divide by N.",
    "Calculate a per-pixel median of the samples to avoid noise. More computationally involved than mean, but often better results.",
    "Use a per-pixel minimum across samples. Usually a good choice for short videos with black backgrounds and individuals that do not move much.",
    "Use a per-pixel maximum across samples. Usually a good choice for short videos with white backgrounds and individuals that do not move much."
)

AveragingAccumulator::AveragingAccumulator() {
    _mode = GlobalSettings::has("averaging_method")
        ?  SETTING(averaging_method).template value<averaging_method_t::Class>()
        : averaging_method_t::mean;
}
AveragingAccumulator::AveragingAccumulator(averaging_method_t::Class mode)
    : _mode(mode)
{ }

void AveragingAccumulator::add(const Mat& f) {
    _add<false>(f);
}

void AveragingAccumulator::add_threaded(const Mat& f) {
    _add<true>(f);
}

template<bool threaded>
void AveragingAccumulator::_add(const Mat &f) {
    const uint8_t channels = f.channels();
    assert(channels == 1 || channels == 3);
    assert(f.type() == CV_8UC1 || f.type() == CV_8UC3);

    {
        std::unique_ptr<std::lock_guard<std::mutex>> guard;
        if constexpr(threaded) {
            guard = std::make_unique<std::lock_guard<std::mutex>>(_accumulator_mutex);
        }
        
        // initialization code
        if(_accumulator.empty()) {
            _size = Size2(f.cols, f.rows);
            _accumulator = cv::Mat::zeros((int)_size.height, (int)_size.width, _mode == averaging_method_t::mean ? CV_32FC(channels) : CV_8UC(channels));
            if(_mode == averaging_method_t::min)
                _accumulator.setTo(255);
            
            if(_mode == averaging_method_t::mode) {
                spatial_histogram.resize(size_t(f.cols) * size_t(f.rows));
                spatial_mutex.resize(size_t(f.cols));
                for(auto &m : spatial_mutex)
                    m = std::make_unique<std::mutex>();
                for(uint64_t i=0; i<spatial_histogram.size(); ++i) {
                    std::fill(spatial_histogram.at(i).begin(), spatial_histogram.at(i).end(), RGB{0, 0, 0});
                }
            }
        }
    }
    
    if(_mode == averaging_method_t::mean) {
        if constexpr(threaded) {
            std::lock_guard guard(_accumulator_mutex);
            f.convertTo(_float_mat, CV_32FC(channels));
            cv::add(_accumulator, _float_mat, _accumulator);
            ++count;
        } else {
            f.convertTo(_float_mat, CV_32FC(channels));
            cv::add(_accumulator, _float_mat, _accumulator);
            ++count;
        }
        
    } else if(_mode == averaging_method_t::mode) {
        assert(f.isContinuous());
        assert(f.type() == CV_8UC(channels));
        
        /// TODO: multiple channels will have weird results here
        const uchar* ptr = (const uchar*)f.data;
        auto array_ptr = spatial_histogram.data();
        const auto end = spatial_histogram.data() + uint64_t(f.cols) * uint64_t(f.rows);
        auto mutex_ptr = spatial_mutex.begin(); //! one lock per line
        //thread_local Timing timing("MutexSwitch", 0.001);
        //thread_local Timing mtiming("MutexLock", 0.001);
        
        assert(spatial_histogram.size() == uint64_t(f.cols) * uint64_t(f.rows));
        if constexpr(threaded) {
            //timing.start_measure();
            (*mutex_ptr)->lock();
            
            for (size_t x = 0; array_ptr != end; ptr += channels, ++array_ptr, ++x) {
                if(x >= size_t(f.cols) * 100) {
                    x = 0;
                    
                    (*mutex_ptr)->unlock();
                    //timing.conclude_measure();
                    
                    //mtiming.start_measure();
                    ++mutex_ptr;
                    (*mutex_ptr)->lock();
                    //mtiming.conclude_measure();
                    
                    //timing.start_measure();
                }
                
                if(channels == 1) {
                    ++(*array_ptr)[*(ptr + 0)][0];
                } else {
                    assert(channels == 3);
                    ++(*array_ptr)[*(ptr + 0)][0];
                    ++(*array_ptr)[*(ptr + 1)][1];
                    ++(*array_ptr)[*(ptr + 2)][2];
                }
            }
            
            if(mutex_ptr == spatial_mutex.end())
                throw RuntimeError("Should not be past end");
            (*mutex_ptr)->unlock();
            //timing.conclude_measure();
            
        } else {
            for (; array_ptr != end; ptr += channels, ++array_ptr) {
                if(channels == 1) {
                    ++(*array_ptr)[*(ptr + 0)][0];
                } else {
                    assert(channels == 3);
                    ++(*array_ptr)[*(ptr + 0)][0];
                    ++(*array_ptr)[*(ptr + 1)][1];
                    ++(*array_ptr)[*(ptr + 2)][2];
                }
            }
        }
        
    } else if(_mode == averaging_method_t::max) {
        if constexpr(threaded) {
            std::lock_guard guard(_accumulator_mutex);
            cv::max(_accumulator, f, _accumulator);
        } else
            cv::max(_accumulator, f, _accumulator);
        
    } else if(_mode == averaging_method_t::min) {
        if constexpr(threaded) {
            std::lock_guard guard(_accumulator_mutex);
            cv::min(_accumulator, f, _accumulator);
        } else
            cv::min(_accumulator, f, _accumulator);
        
    } else
        throw U_EXCEPTION("Unknown averaging_method ", _mode.name());
}

std::unique_ptr<cmn::Image> AveragingAccumulator::finalize() {
    std::lock_guard guard(_accumulator_mutex);
    auto image = std::make_unique<cmn::Image>(_accumulator.rows, _accumulator.cols, _accumulator.channels());
    
    if(_mode == averaging_method_t::mean) {
        cv::divide(_accumulator, double(count), _local);
        _local.convertTo(image->get(), CV_8UC(_local.channels()));
        
    } else if(_mode == averaging_method_t::mode) {
        //_accumulator.copyTo(image->get());
        
        auto ptr = image->data();
        auto array_ptr = spatial_histogram.data();
        const auto end = spatial_histogram.data() + uint64_t(image->cols) * uint64_t(image->rows);
        
        const uint8_t channels = image->channels();
        for (; array_ptr != end; ptr += channels, ++array_ptr) {
            if(channels == 1) {
                *ptr = std::distance(array_ptr->begin(), std::max_element(array_ptr->begin(), array_ptr->end(), [](const RGB& A, const RGB& B) {
                    return A[0] < B[0];
                }));
            } else {
                /// this is very inefficient
                assert(channels == 3);
                *(ptr + 0) = std::distance(array_ptr->begin(), std::max_element(array_ptr->begin(), array_ptr->end(), [](const RGB& A, const RGB& B) {
                    return A[0] < B[0];
                }));
                *(ptr + 1) = std::distance(array_ptr->begin(), std::max_element(array_ptr->begin(), array_ptr->end(), [](const RGB& A, const RGB& B) {
                    return A[1] < B[1];
                }));
                *(ptr + 2) = std::distance(array_ptr->begin(), std::max_element(array_ptr->begin(), array_ptr->end(), [](const RGB& A, const RGB& B) {
                    return A[2] < B[2];
                }));
            }
        }
        
    } else
        _accumulator.copyTo(image->get());
    
    //tf::imshow("acc_average", image->get());
    
    return image;
}

}
