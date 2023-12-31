//
// Created by DefTruth on 2022/6/29.
//

#ifndef LITE_AI_TOOLKIT_ORT_CV_FACE_PARSING_BISENET_H
#define LITE_AI_TOOLKIT_ORT_CV_FACE_PARSING_BISENET_H

#include "ort_basehandler.h"
#include "ort_config.h"

typedef struct FaceParsingContentType
{
    cv::Mat label; // integer labels 0 ~ 255 | 0 ~ 19 | ... so on
    cv::Mat merge; // merge mat with original image and mask
    bool flag;

    FaceParsingContentType() : flag(false)
    {};
} FaceParsingContent;

namespace ortcv
{
    class FaceParsingBiSeNet : public core::BasicOrtHandler
    {
    public:
        explicit FaceParsingBiSeNet(const std::string& _onnx_path, unsigned int _num_threads = 1) :
            BasicOrtHandler(_onnx_path, _num_threads)
        {};

        ~FaceParsingBiSeNet() override = default;

    private:
        const float mean_vals[3] = { 0.485f * 255.f, 0.456f * 255.f, 0.406f * 255.f }; // RGB
        const float scale_vals[3] = { 1.f / (0.229f * 255.f), 1.f / (0.224f * 255.f), 1.f / (0.225f * 255.f) };

    private:
        Ort::Value transform(const cv::Mat& mat) override;

        void generate_mask(std::vector<Ort::Value>& output_tensors,
            const cv::Mat& mat, FaceParsingContent& content,
            bool minimum_post_process = false);

    public:
        void detect(const cv::Mat& mat, FaceParsingContent& content,
            bool minimum_post_process = false);
    };
}

#endif //LITE_AI_TOOLKIT_ORT_CV_FACE_PARSING_BISENET_H
