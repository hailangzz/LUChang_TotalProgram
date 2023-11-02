/*
 * @Author: chenjingyu
 * @Date: 2023-07-30 12:56:03
 * @LastEditTime: 2023-07-30 12:57:55
 * @Description: Face landmark detector
 * @FilePath: \Mediapipe-MNN\source\FaceLandmarkDetector.h
 */
#pragma once

#include "TypeDefines.h"
#include <vector>
#include <memory>
#include <MNN/Matrix.h>
#include <MNN/ImageProcess.hpp>
#include <MNN/Interpreter.hpp>
#include <MNN/Tensor.hpp>
#include <opencv2/core.hpp>

namespace dms
{
	struct LandmarkGroup
	{
		std::vector<ushort> eye_left
		{
			362, 398, 384, 385, 386, 387, 388, 466, // 362 左侧眼角
			263, 249, 390, 373, 374, 380, 381, 382  // 263 右侧眼角
		};

		std::vector<ushort> eye_right
		{
			 33, 246, 161, 160, 159, 158, 157, 173, //  33 左侧眼角
			133, 155, 154, 153, 145, 144, 163,   7  // 133 右侧眼角
		};

		std::vector<ushort> mouth
		{
			 78, 191,  80,  81,  82, 13, 312, 311, 310, 415, //  78 左侧嘴角
			308, 324, 318, 402, 317, 14,  87, 178,  88,  95  // 308 右侧嘴角
		};

		std::vector<ushort> head_pose_idx
		{	
			// 用于估计头部姿态的关键点（相对稳定，不易受面部表情影响）
			//162,  21,  54, 103,  67, 109,  10, 338, 297, 332, 284, 251, 389,
			//368, 301, 298, 333, 299, 337, 151, 108,  69, 104,  68,  71, 139,
			//107,   9, 336, 285,   8,  55, 193, 168, 417, 351,   6, 122

			162,  21,  54, 103,  67, 109,  10, 338, 297, 332, 284, 251, 389,
			368, 301, 298, 333, 299, 337, 151, 108,  69, 104,  68,  71, 139,
			107,   9, 336, 285,   8,  55, 193, 168, 417, 351,   6, 122

			// 面部轮廓 (姿态估计偏差大)
			//10,  338, 297, 332, 284, 251, 389, 356, 454, 323, 361, 288, 397, 365, 379, 378, 400, 377,
			//152, 148, 176, 149, 150, 136, 172,  58, 132,  93, 234, 127, 162,  27,  54, 103,  67, 109
		};
	};

	class FaceLandmarkDetector
	{
	public:
		FaceLandmarkDetector() = default;
		~FaceLandmarkDetector();

		bool loadModel(const char* model_file);
		void setFormat(int source_format);
		bool detect(const ImageHead& in, RotateType type, std::vector<ObjectInfo>& objects);
		
		void drawFaceMesh(cv::Mat& src, const std::vector<Point3f>& landmarks_3D);
		void drawLeftEyeContour(cv::Mat& src, const std::vector<Point3f>& landmarks_3D);
		void drawRightEyeContour(cv::Mat& src, const std::vector<Point3f>& landmarks_3D);
		void drawMouthContour(cv::Mat& src, const std::vector<Point3f>& landmarks_3D);

	private:
		bool inited_ = false;
		int input_c_ = 0;
		int input_w_ = 0;
		int input_h_ = 0;
		std::shared_ptr<MNN::CV::ImageProcess> pretreat_ = nullptr;
		std::unique_ptr<MNN::Interpreter> net_ = nullptr;
		MNN::Session* session_ = nullptr;
		MNN::Tensor* input_tensor_ = nullptr;
		MNN::CV::Matrix trans_;

		const float meanVals_[3] = { 0.0f, 0.0f, 0.0f };
		const float normVals_[3] = { 1 / 255.f, 1 / 255.f, 1 / 255.f };
		LandmarkGroup landmark_group_;
	};
} // namespace mirror
