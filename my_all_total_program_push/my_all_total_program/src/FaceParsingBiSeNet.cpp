//
// Created by DefTruth on 2022/6/29.
//

#include "FaceParsingBiSeNet.h"
#include "ort_utils.h"

using ortcv::FaceParsingBiSeNet;

Ort::Value FaceParsingBiSeNet::transform(const cv::Mat& mat)
{
    cv::Mat canvas;
    cv::resize(mat, canvas, cv::Size(input_node_dims.at(3), input_node_dims.at(2)));
    cv::cvtColor(canvas, canvas, cv::COLOR_BGR2RGB);
    // e.g (1,3,512,512)
    ortcv::utils::transform::normalize_inplace(canvas, mean_vals, scale_vals);
    return ortcv::utils::transform::create_tensor(
        canvas, input_node_dims, memory_info_handler,
        input_values_handler, ortcv::utils::transform::CHW); // deepcopy inside
}

void FaceParsingBiSeNet::detect(const cv::Mat& mat, FaceParsingContent& content,
    bool minimum_post_process)
{
    if (mat.empty()) return;

    // 1. make input tensor
    Ort::Value input_tensor = this->transform(mat);
    // 2. inference
    auto output_tensors = ort_session->Run(
        Ort::RunOptions{nullptr}, input_node_names.data(),
        & input_tensor, 1, output_node_names.data(), num_outputs
    );
    // 3. generate mask
    this->generate_mask(output_tensors, mat, content, minimum_post_process);
}

static inline uchar argmax(float* mutable_ptr, const unsigned int& step)
{
    std::vector<float> logits(19, 0.f);
    for (unsigned int i = 0; i < 19; ++i)
        logits[i] = *(mutable_ptr + i * step);
    uchar label = 0;
    float max_logit = logits[0];
    for (unsigned int i = 1; i < 19; ++i)
    {
        if (logits[i] > max_logit)
        {
            max_logit = logits[i];
            label = (uchar)i;
        }
    }
    return label;
}

static const uchar part_colors[20][3] = {
    {255, 0,   0},
    {255, 85,  0},
    {255, 170, 0},
    {255, 0,   85},
    {255, 0,   170},
    {0,   255, 0},
    {85,  255, 0},
    {170, 255, 0},
    {0,   255, 85},
    {0,   255, 170},
    {0,   0,   255},
    {85,  0,   255},
    {170, 0,   255},
    {0,   85,  255},
    {0,   170, 255},
    {255, 255, 0},
    {255, 255, 85},
    {255, 255, 170},
    {255, 0,   255},
    {255, 85,  255}
};

void FaceParsingBiSeNet::generate_mask(std::vector<Ort::Value>& output_tensors, const cv::Mat& mat,
    FaceParsingContent& content,
    bool minimum_post_process)
{
    Ort::Value& output = output_tensors.at(0); // (1,19,h,w)
    const unsigned int h = mat.rows;
    const unsigned int w = mat.cols;

    auto output_dims = output.GetTypeInfo().GetTensorTypeAndShapeInfo().GetShape();
    const unsigned int out_h = output_dims.at(2);
    const unsigned int out_w = output_dims.at(3);
    const unsigned int channel_step = out_h * out_w;

    float* output_ptr = output.GetTensorMutableData<float>();
    std::vector<uchar> elements(channel_step, 0); // allocate
    for (unsigned int i = 0; i < channel_step; ++i)
        elements[i] = argmax(output_ptr + i, channel_step);

    cv::Mat label(out_h, out_w, CV_8UC1, elements.data());

    if (!minimum_post_process)
    {
        // FaceParsingBiSeNet only predict integer label mask,
        // no fgr. So, the fake fgr and merge mat may not need,
        // let the fgr mat and merge mat empty to
        // Speed up the post processes.
        const uchar* label_ptr = label.data;
        cv::Mat color_mat(out_h, out_w, CV_8UC3, cv::Scalar(255, 255, 255));
        for (unsigned int i = 0; i < color_mat.rows; ++i)
        {
            cv::Vec3b* p = color_mat.ptr<cv::Vec3b>(i);
            for (unsigned int j = 0; j < color_mat.cols; ++j)
            {
                if (label_ptr[i * out_w + j] == 0) continue;
                p[j][0] = part_colors[label_ptr[i * out_w + j]][0];
                p[j][1] = part_colors[label_ptr[i * out_w + j]][1];
                p[j][2] = part_colors[label_ptr[i * out_w + j]][2];
            }
        }
        if (out_h != h || out_w != w)
            cv::resize(color_mat, color_mat, cv::Size(w, h));
        cv::addWeighted(mat, 0.4, color_mat, 0.6, 0., content.merge);
    }
    // already allocated a new continuous memory after resize.
    if (out_h != h || out_w != w) cv::resize(label, label, cv::Size(w, h));
    // need clone to allocate a new continuous memory if not performed resize.
    // The memory elements point to will release after return.
    else label = label.clone();

    content.label = label; // auto handle the memory inside ocv with smart ref.
    content.flag = true;
}









































