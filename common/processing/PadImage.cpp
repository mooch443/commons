#include "PadImage.h"

namespace cmn {
Vec2 legacy_pad_image(cv::Mat& padded, Size2 output_size) {
    Vec2 offset;
    int left = 0, right = 0, top = 0, bottom = 0;
    if(padded.cols < output_size.width) {
        left = roundf(output_size.width - padded.cols);
        right = left / 2;
        left -= right;
    }
    
    if(padded.rows < output_size.height) {
        top = roundf(output_size.height - padded.rows);
        bottom = top / 2;
        top -= bottom;
    }
    
    if(left || right || top || bottom) {
        offset.x -= left;
        offset.y -= top;
        
        cv::copyMakeBorder(padded, padded, top, bottom, left, right, cv::BORDER_CONSTANT, 0);
    }
    
    assert(padded.cols >= output_size.width && padded.rows >= output_size.height);
    if(padded.cols > output_size.width || padded.rows > output_size.height) {
        left = padded.cols - output_size.width;
        right = left / 2;
        left -= right;
        
        top = padded.rows - output_size.height;
        bottom = top / 2;
        top -= bottom;
        
        offset.x += left;
        offset.y += top;
        
        padded(Bounds(left, top, padded.cols - left - right, padded.rows - top - bottom)).copyTo(padded);
    }
    return offset;
}

    void pad_image(const cv::Mat& input, cv::Mat& output, const Size2& target, int dtype, bool reset, const cv::Mat& mask, uchar background)
    {
        assert(&input != &output);
        dtype = dtype == -1 ? input.type() : dtype;
        assert(reset || (output.cols == target.width && output.rows == target.height));
        if(reset || output.cols != target.width || output.rows != target.height)
            output = cv::Mat::zeros(target.height, target.width, dtype);
        if(background != 0)
            output.setTo(background);
        
        if(input.cols > output.cols || input.rows > output.rows) {
            Size2 ratio_size(target);
            
            /**
             
             (5,5) -> (2,2)
             (10,5) -> (10,2)
             
             **/
            if (input.cols - output.cols >= input.rows - output.rows) {
                float ratio = input.rows / float(input.cols);
                ratio_size.width = output.cols;
                ratio_size.height = roundf(ratio * output.cols);
                
            } else {
                float ratio = input.cols / float(input.rows);
                ratio_size.width = roundf(ratio * output.rows);
                ratio_size.height = output.rows;
            }
            
            assert(ratio_size.width <= target.width && ratio_size.height <= target.height);
            
            //size_t left = (output.cols - ratio_size.width) * 0.5,
            //        top = (output.rows - ratio_size.height) * 0.5;
            
            throw U_EXCEPTION("Resize is not allowed.");
            
            /*if(dtype == input.type()) {
                assert(mask.empty());
                cv::resize(input, output(cv::Rect(left, top, ratio_size.width, ratio_size.height)), cv::Size(ratio_size));
            }
            else {
                assert(mask.empty());
                cv::Mat tmp;
                cv::resize(input, tmp, cv::Size(ratio_size));
                tmp.convertTo(output(cv::Rect(left, top, ratio_size.width, ratio_size.height)), dtype, dtype & CV_32F || dtype & CV_64F ? 1./255.f : 1);
            }*/
            
        } else {
            size_t left = (output.cols - input.cols) * 0.5,
                    top = (output.rows - input.rows) * 0.5;
            
            if(!mask.empty()) {
                assert(mask.cols == input.cols);
                assert(mask.rows == input.rows);
                
                if(dtype != input.type()) {
                    cv::Mat tmp;
                    input.convertTo(tmp, dtype, dtype & CV_32F || dtype & CV_64F ? 1./255.f : 1);
                    tmp.copyTo(output(Bounds(left, top, input.cols, input.rows)), mask);
                }
                else
                    input.copyTo(output(Bounds(left, top, input.cols, input.rows)), mask);
                
            } else {
                if(dtype != input.type())
                    input.convertTo(output(Bounds(left, top, input.cols, input.rows)), dtype, dtype & CV_32F || dtype & CV_64F ? 1./255.f : 1);
                else
                    input.copyTo(output(Bounds(left, top, input.cols, input.rows)));
            }
        }
    }
}
