#pragma once
#ifndef LITE_AI_TOOLKIT_NCNN_CV_NCNN_PFLD_H
#define LITE_AI_TOOLKIT_NCNN_CV_NCNN_PFLD_H

#include "ncnn_basehandle.h"
#include "pfld.h"


namespace ncnncv
{
    class  NCNNPFLD : public ncnncore::BasicNCNNHandler
    {
    public:
        explicit NCNNPFLD(const std::string& _param_path,
            const std::string& _bin_path,
            unsigned int _num_threads = 1);

        ~NCNNPFLD() override = default;

    private:
        const int input_height = 112;
        const int input_width = 112;
        const float mean_vals[3] = { 0.f, 0.f, 0.f };
        const float norm_vals[3] = { 1.0f / 255.f, 1.0f / 255.f, 1.0f / 255.f };

    private:
        void transform(const cv::Mat& mat, ncnn::Mat& in) override;

    public:
        void detect(const cv::Mat& mat, Landmarks& landmarks);
    };
}

#endif //LITE_AI_TOOLKIT_NCNN_CV_NCNN_PFLD_H
