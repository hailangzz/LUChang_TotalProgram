#pragma once
#ifndef LITE_AI_ORT_CV_PFLD_H
#define LITE_AI_ORT_CV_PFLD_H

#include "ort_basehandler.h"

typedef struct  LandmarksType
{
    std::vector<cv::Point2f> points; // x,y
    bool flag;

    LandmarksType() : flag(false)
    {};
} Landmarks;


namespace ortcv
{

    class  PFLD : public core::BasicOrtHandler
    {
    private:
        static constexpr const float mean_val = 0.f;
        static constexpr const float scale_val = 1.0f / 255.0f;

    public:
        explicit PFLD(const std::string& _onnx_path, unsigned int _num_threads = 1) :
            BasicOrtHandler(_onnx_path, _num_threads)
        {};

        ~PFLD() override = default; // override

    private:
        Ort::Value transform(const cv::Mat& mat) override;

    public:
        void detect(const cv::Mat& mat, Landmarks& landmarks);

    };
}


#endif //LITE_AI_ORT_CV_PFLD_H
