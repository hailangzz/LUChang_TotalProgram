//
// Created by DefTruth on 2021/3/14.
//

#ifndef LITE_AI_ORT_CV_FSANET_H
#define LITE_AI_ORT_CV_FSANET_H

#include "ort_basehandler.h"

typedef struct EulerAnglesType
{
    float yaw;
    float pitch;
    float roll;
    bool flag;

    EulerAnglesType() : flag(false)
    {};
} EulerAngles;

namespace ortcv
{

    class FSANet : public core::BasicOrtHandler
    {

    private:
        static constexpr const float pad = 0.3f;
        static constexpr const int input_width = 64;
        static constexpr const int input_height = 64;

    public:
        explicit FSANet(const std::string& _onnx_path, unsigned int _num_threads = 1) :
            BasicOrtHandler(_onnx_path, _num_threads)
        {};

        ~FSANet() override = default; // override

    private:
        Ort::Value transform(const cv::Mat& mat) override; //  padding & resize & normalize.

    public:
        void detect(const cv::Mat& mat, EulerAngles& euler_angles);
    };

}

#endif //LITE_AI_ORT_CV_FSANET_H
