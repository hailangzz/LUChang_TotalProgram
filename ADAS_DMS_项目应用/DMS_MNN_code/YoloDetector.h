#ifndef YOLODETECTOR_H
#define YOLODETECTOR_H

#include <MNN/MNNDefine.h>
#include <MNN/MNNForwardType.h>
#include <MNN/Interpreter.hpp>
#include <opencv2/opencv.hpp>
#include "util/Utils.h"
#include "TypeDefines.h"

namespace dms {

	 struct YoloModelConfig{
		
		int model_input_width = 384;
		int model_input_height = 384;
		int num_classes =3;
		YoloSize yolosize = YoloSize{ model_input_width,model_input_height };
		std::vector<YoloLayerData> yolo_layers = {
			{"381", 32, { {116, 90}, {156, 198}, {373, 326} }},
			{ "364",    16, {{30,  61}, {62,  45},  {59,  119}} },
			{ "output0", 8,  {{10,  13}, {16,  30},  {33,  23}} },
		};

		float threshold = 0.25;     // classification confience threshold
		float nms_threshold = 0.5;  // nms confience threshold		

	};
	


	class YoloDetector {
	public:
		YoloDetector() = default;
		~YoloDetector();

		bool loadModel(const char* model_file);
		bool GetInputImageIntoTensor(cv::Mat frame);

		
		bool detect(cv::Mat frame,
			std::vector<BoxInfo>& objects);

		std::vector<BoxInfo>
			decodeInfer(MNN::Tensor& data, int stride, const YoloSize& frame_size, int num_classes,
				const std::vector<YoloSize>& anchors, float threshold);

	

	private:
		YoloModelConfig yolo_model_config_;
		std::shared_ptr<MNN::Interpreter> net_ = nullptr;
		MNN::Session* session_ = nullptr;
		MNN::Tensor* input_tensor_ = nullptr;


		bool inited_ = false;
		int input_c_ = 1;
		int input_w_ = 0;
		int input_h_ = 0;
		
	};
	
}

#endif //YOLODETECTOR_H