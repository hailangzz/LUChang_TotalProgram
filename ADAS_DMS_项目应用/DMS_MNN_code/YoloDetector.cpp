#include "YoloDetector.h"
#include "CommonData.h"
#include "util/Utils.h"
#include <iostream>


using namespace MNN;

namespace dms {
	YoloDetector::~YoloDetector() {
		net_->releaseModel();
		net_->releaseSession(session_);
	}

	bool YoloDetector::loadModel(const char* model_file)
	{
		// 1. Load model
		net_ = std::unique_ptr<Interpreter>(Interpreter::createFromFile(model_file));
		if (net_ == nullptr) {
			CVLOGE("Failed to load yolo detection model.");
			return false;
		}

		// 2. Create session
		ScheduleConfig schedule_config;
		schedule_config.type = MNN_FORWARD_CPU;
		schedule_config.numThread = 4;
		session_ = net_->createSession(schedule_config);
		input_tensor_ = net_->getSessionInput(session_, nullptr);

		input_c_ = input_tensor_->channel();
		input_h_ = input_tensor_->height();
		input_w_ = input_tensor_->width();		

		CVLOGD("yolo detection model loaded, input size:(%d, %d, %d).\n", input_c_, input_h_, input_w_);
		inited_ = true;
		return true;
	}

	bool YoloDetector::GetInputImageIntoTensor(cv::Mat frame)
	{
		cv::Mat image;
		cv::resize(frame, image, cv::Size(yolo_model_config_.model_input_width, yolo_model_config_.model_input_height));
		// preprocessing
		image.convertTo(image, CV_32FC3);
		// image = (image * 2 / 255.0f) - 1;
		image = image / 255.0f;
		// wrapping input tensor, convert nhwc to nchw    
		std::vector<int> dims{1, yolo_model_config_.model_input_width, yolo_model_config_.model_input_height, 3};
		auto nhwc_Tensor = MNN::Tensor::create<float>(dims, NULL, MNN::Tensor::TENSORFLOW);
		auto nhwc_data = nhwc_Tensor->host<float>();
		auto nhwc_size = nhwc_Tensor->size();
		std::memcpy(nhwc_data, image.data, nhwc_size);
		input_tensor_->copyFromHostTensor(nhwc_Tensor);
	}

	std::vector<BoxInfo> YoloDetector::decodeInfer(MNN::Tensor& data, int stride, const YoloSize& frame_size, int num_classes,
		const std::vector<YoloSize>& anchors, float threshold) {
		std::vector<BoxInfo> result;
		int batchs, channels, height, width, pred_item;
		batchs = data.shape()[0];
		channels = data.shape()[1];
		height = data.shape()[2];
		width = data.shape()[3];
		pred_item = data.shape()[4];

		auto data_ptr = data.host<float>();
		for (int bi = 0; bi < batchs; bi++)
		{
			auto batch_ptr = data_ptr + bi * (channels * height * width * pred_item);
			for (int ci = 0; ci < channels; ci++)
			{
				auto channel_ptr = batch_ptr + ci * (height * width * pred_item);
				for (int hi = 0; hi < height; hi++)
				{
					auto height_ptr = channel_ptr + hi * (width * pred_item);
					for (int wi = 0; wi < width; wi++)
					{
						auto width_ptr = height_ptr + wi * pred_item;
						auto cls_ptr = width_ptr + 5;

						auto confidence = sigmoid(width_ptr[4]);
						//std::cout<<"yolo object confidence:"<< confidence << std::endl;

						for (int cls_id = 0; cls_id < num_classes; cls_id++)
						{
							float score = sigmoid(cls_ptr[cls_id]) * confidence;
							//std::cout << "yolo object score:" << score << std::endl;
							
							if (score > threshold)
							{
								float cx = (sigmoid(width_ptr[0]) * 2.f - 0.5f + wi) * (float)stride;
								float cy = (sigmoid(width_ptr[1]) * 2.f - 0.5f + hi) * (float)stride;
								float w = pow(sigmoid(width_ptr[2]) * 2.f, 2) * anchors[ci].width;
								float h = pow(sigmoid(width_ptr[3]) * 2.f, 2) * anchors[ci].height;

								BoxInfo box;

								box.x1 = std::max(0, std::min(frame_size.width, int((cx - w / 2.f))));
								box.y1 = std::max(0, std::min(frame_size.height, int((cy - h / 2.f))));
								box.x2 = std::max(0, std::min(frame_size.width, int((cx + w / 2.f))));
								box.y2 = std::max(0, std::min(frame_size.height, int((cy + h / 2.f))));
								box.score = score;
								box.label = cls_id;
								result.push_back(box);
								//std::cout << "yolo object confidence:" << confidence << std::endl;
								//std::cout << "yolo object score:" << score << std::endl;
								//std::cout << "box.score:" << box.score << std::endl;
							}
						}
					}
				}
			}
		}

		return result;

	}

	bool YoloDetector::detect(cv::Mat frame,std::vector<BoxInfo>& objects)
	{	
		// get frame image into network Tensor
		GetInputImageIntoTensor(frame);

		// run network

		net_->runSession(session_);
		
		// get output data
		std::string output_tensor_name0 = yolo_model_config_.yolo_layers[2].name;
		std::string output_tensor_name1 = yolo_model_config_.yolo_layers[1].name;
		std::string output_tensor_name2 = yolo_model_config_.yolo_layers[0].name;

		MNN::Tensor* tensor_scores = net_->getSessionOutput(session_, output_tensor_name0.c_str());
		MNN::Tensor* tensor_boxes = net_->getSessionOutput(session_, output_tensor_name1.c_str());
		MNN::Tensor* tensor_anchors = net_->getSessionOutput(session_, output_tensor_name2.c_str());

		MNN::Tensor tensor_scores_host(tensor_scores, tensor_scores->getDimensionType());
		MNN::Tensor tensor_boxes_host(tensor_boxes, tensor_boxes->getDimensionType());
		MNN::Tensor tensor_anchors_host(tensor_anchors, tensor_anchors->getDimensionType());

		tensor_scores->copyToHostTensor(&tensor_scores_host);
		tensor_boxes->copyToHostTensor(&tensor_boxes_host);
		tensor_anchors->copyToHostTensor(&tensor_anchors_host);

		std::vector<BoxInfo> boxes;


		boxes = decodeInfer(tensor_scores_host, yolo_model_config_.yolo_layers[2].stride, yolo_model_config_.yolosize, yolo_model_config_.num_classes, yolo_model_config_.yolo_layers[2].anchors, yolo_model_config_.threshold);
		objects.insert(objects.begin(), boxes.begin(), boxes.end());

		boxes = decodeInfer(tensor_boxes_host, yolo_model_config_.yolo_layers[1].stride, yolo_model_config_.yolosize, yolo_model_config_.num_classes, yolo_model_config_.yolo_layers[1].anchors, yolo_model_config_.threshold);
		objects.insert(objects.begin(), boxes.begin(), boxes.end());

		boxes = decodeInfer(tensor_anchors_host, yolo_model_config_.yolo_layers[0].stride, yolo_model_config_.yolosize, yolo_model_config_.num_classes, yolo_model_config_.yolo_layers[0].anchors, yolo_model_config_.threshold);
		objects.insert(objects.begin(), boxes.begin(), boxes.end());
		


		YoloNMS(objects, yolo_model_config_.nms_threshold);
		//for (const auto& box_info : objects) {
		//	std::cout << "box_info.label" << box_info.label << "box_info.score" << box_info.score << std::endl;
		//}
	}

	void YoloDetector::ScaleCoords(std::vector<BoxInfo>& object, int width_image_origin, int height_image_origin) {
		float w_ratio = float(width_image_origin) / float(yolo_model_config_.model_input_width);
		float h_ratio = float(height_image_origin) / float(yolo_model_config_.model_input_height);


		for (auto& box : object)
		{
			box.x1 *= w_ratio;
			box.x2 *= w_ratio;
			box.y1 *= h_ratio;
			box.y2 *= h_ratio;
		}
		return;
	}

}
