#include <iostream>
#include <string>
#include <ctime>

#include <MNN/MNNDefine.h>
#include <MNN/MNNForwardType.h>
#include <MNN/Interpreter.hpp>
#include <opencv2/opencv.hpp>

#include "Yolo.h"

void show_shape(std::vector<int> shape)
{
    std::cout << shape[0] << " " << shape[1] << " " << shape[2] << " " << shape[3] << " " << shape[4] << " " << std::endl;

}

void scale_coords(std::vector<BoxInfo>& boxes, int w_from, int h_from, int w_to, int h_to)
{
    float w_ratio = float(w_to) / float(w_from);
    float h_ratio = float(h_to) / float(h_from);


    for (auto& box : boxes)
    {
        box.x1 *= w_ratio;
        box.x2 *= w_ratio;
        box.y1 *= h_ratio;
        box.y2 *= h_ratio;
    }
    return;
}

cv::Mat draw_box(cv::Mat& cv_mat, std::vector<BoxInfo>& boxes, int CNUM)
{
    static const char* class_names[] = {
        "0", "1", "2"
    };
    cv::RNG rng(0xFFFFFFFF);
    cv::Scalar_<int> randColor[3];
    for (int i = 0; i < CNUM; i++)
        rng.fill(randColor[i], cv::RNG::UNIFORM, 0, 256);

    for (auto box : boxes)
    {
        int width = box.x2 - box.x1;
        int height = box.y2 - box.y1;
        // int id = box.id;
        char text[256];
        cv::Point p = cv::Point(box.x1, box.y1 - 5);
        cv::Rect rect = cv::Rect(box.x1, box.y1, width, height);
        cv::rectangle(cv_mat, rect, cv::Scalar(0, 0, 255));
        printf(text, "%s %.1f%%", class_names[box.label], box.score * 100);
        cv::putText(cv_mat, text, p, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255));
    }
    return cv_mat;
}


int main(int argc, char* argv[])
{
    //   std::string model_name = "./model_mnn/yolov5s.mnn";
    std::string model_name = "./model_mnn/best.mnn";


    int INPUT_SIZE = 384;      // the size of input images
    std::string image_name = "./images/000070.jpg";



    int num_classes = 3;       // the number of classes

    /*
    std::vector<YoloLayerData> yolov5s_layers{
        {"482", 32, { {116, 90}, {156, 198}, {373, 326} }},
        { "416",    16, {{30,  61}, {62,  45},  {59,  119}} },
        { "output", 8,  {{10,  13}, {16,  30},  {33,  23}} },
    };
    */

    std::vector<YoloLayerData> yolov5s_layers{
        {"277", 32, { {116, 90}, {156, 198}, {373, 326} }},
        { "275",    16, {{30,  61}, {62,  45},  {59,  119}} },
        { "output", 8,  {{10,  13}, {16,  30},  {33,  23}} },
    };
    std::vector<YoloLayerData>& layers = yolov5s_layers;

    std::shared_ptr<MNN::Interpreter> net = std::shared_ptr<MNN::Interpreter>(MNN::Interpreter::createFromFile(model_name.c_str()));
    if (nullptr == net) {
        return 0;
    }

    MNN::ScheduleConfig config;
    config.numThread = 4;
    config.type = static_cast<MNNForwardType>(MNN_FORWARD_CPU);
    MNN::BackendConfig backendConfig;
    backendConfig.precision = (MNN::BackendConfig::PrecisionMode)2;
    // backendConfig.precision =  MNN::PrecisionMode Precision_Normal; // static_cast<PrecisionMode>(Precision_Normal);
    config.backendConfig = &backendConfig;
    MNN::Session* session = net->createSession(config);;


    // load image
    cv::Mat raw_image = cv::imread(image_name.c_str());
    cv::Mat image;
    cv::resize(raw_image, image, cv::Size(INPUT_SIZE, INPUT_SIZE));

    // preprocessing
    image.convertTo(image, CV_32FC3);
    // image = (image * 2 / 255.0f) - 1;
    image = image / 255.0f;

    // wrapping input tensor, convert nhwc to nchw    
    std::vector<int> dims{1, INPUT_SIZE, INPUT_SIZE, 3};
    auto nhwc_Tensor = MNN::Tensor::create<float>(dims, NULL, MNN::Tensor::TENSORFLOW);
    auto nhwc_data = nhwc_Tensor->host<float>();
    auto nhwc_size = nhwc_Tensor->size();
    std::memcpy(nhwc_data, image.data, nhwc_size);

    auto inputTensor = net->getSessionInput(session, nullptr);
    inputTensor->copyFromHostTensor(nhwc_Tensor);

    // run network
    clock_t startTime, endTime;
    startTime = clock();    // 计时开始
    net->runSession(session);
    endTime = clock();     // 计时结束
    cout << "The forward time is: " << (double)(endTime - startTime) / 1000.0 << "ms" << endl;

    // get output data
    std::string output_tensor_name0 = layers[2].name;
    std::string output_tensor_name1 = layers[1].name;
    std::string output_tensor_name2 = layers[0].name;


    MNN::Tensor* tensor_scores = net->getSessionOutput(session, output_tensor_name0.c_str());
    MNN::Tensor* tensor_boxes = net->getSessionOutput(session, output_tensor_name1.c_str());
    MNN::Tensor* tensor_anchors = net->getSessionOutput(session, output_tensor_name2.c_str());

    MNN::Tensor tensor_scores_host(tensor_scores, tensor_scores->getDimensionType());
    MNN::Tensor tensor_boxes_host(tensor_boxes, tensor_boxes->getDimensionType());
    MNN::Tensor tensor_anchors_host(tensor_anchors, tensor_anchors->getDimensionType());

    tensor_scores->copyToHostTensor(&tensor_scores_host);
    tensor_boxes->copyToHostTensor(&tensor_boxes_host);
    tensor_anchors->copyToHostTensor(&tensor_anchors_host);

    std::vector<BoxInfo> result;
    std::vector<BoxInfo> boxes;

    yolocv::YoloSize yolosize = yolocv::YoloSize{ INPUT_SIZE,INPUT_SIZE };

    float threshold = 0.25;     // classification confience threshold
    float nms_threshold = 0.5;  // nms confience threshold

    show_shape(tensor_scores_host.shape());
    show_shape(tensor_boxes_host.shape());
    show_shape(tensor_anchors_host.shape());


    boxes = decode_infer(tensor_scores_host, layers[2].stride, yolosize, INPUT_SIZE, num_classes, layers[2].anchors, threshold);
    result.insert(result.begin(), boxes.begin(), boxes.end());

    boxes = decode_infer(tensor_boxes_host, layers[1].stride, yolosize, INPUT_SIZE, num_classes, layers[1].anchors, threshold);
    result.insert(result.begin(), boxes.begin(), boxes.end());

    boxes = decode_infer(tensor_anchors_host, layers[0].stride, yolosize, INPUT_SIZE, num_classes, layers[0].anchors, threshold);
    result.insert(result.begin(), boxes.begin(), boxes.end());

    nms(result, nms_threshold);

    std::cout << result.size() << std::endl;     // print the number of prediction

    scale_coords(result, INPUT_SIZE, INPUT_SIZE, raw_image.cols, raw_image.rows);
    cv::Mat frame_show = draw_box(raw_image, result, num_classes);
    cv::imwrite("predict_result.jpg", frame_show);   // save output result  
    return 0;
}