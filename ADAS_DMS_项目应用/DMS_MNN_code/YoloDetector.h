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
		
		int model_input_width = 384;											// 模型输入宽度
		int model_input_height = 384;											// 模型输入高度
		int num_classes =3;														// 检测目标个数
		YoloSize yolosize = YoloSize{ model_input_width,model_input_height };	// 模型输入宽、高尺寸
		std::vector<YoloLayerData> yolo_layers = {								// 模型输出层节点名、下采样倍数、anchor信息
			{"381", 32, { {116, 90}, {156, 198}, {373, 326} }},
			{ "364",    16, {{30,  61}, {62,  45},  {59,  119}} },
			{ "output0", 8,  {{10,  13}, {16,  30},  {33,  23}} },
		};

		float threshold = 0.33;     // classification confience threshold        // 目标检测阈值
		float nms_threshold = 0.5;  // nms confience threshold					 // 极大值抑制阈值

	};
	


	class YoloDetector {
	public:
		YoloDetector() = default;
		~YoloDetector();

		bool loadModel(const char* model_file);									// 加载模型
		bool GetInputImageIntoTensor(cv::Mat frame);							// 读取图像矩阵到InputTensor中

		
		bool detect(cv::Mat frame,                                              // 执行网络检测推理
			std::vector<BoxInfo>& objects);

		std::vector<BoxInfo>
			decodeInfer(MNN::Tensor& data, int stride, const YoloSize& frame_size, int num_classes,
				const std::vector<YoloSize>& anchors, float threshold);

		void ScaleCoords(std::vector<BoxInfo>& object, int width_image_origin, int height_image_origin);

	private:
		YoloModelConfig yolo_model_config_;									    // 网络基本配置变量
		std::shared_ptr<MNN::Interpreter> net_ = nullptr;						// yolo net_
		MNN::Session* session_ = nullptr;										// 网络session_
		MNN::Tensor* input_tensor_ = nullptr;                                   // tensor 输入


		bool inited_ = false;													// 网络初始化标志
		int input_c_ = 1;
		int input_w_ = 0;
		int input_h_ = 0;
		
	};
	
}

#endif //YOLODETECTOR_H