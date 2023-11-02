/*
 * @Author: chenjingyu
 * @Date: 2023-07-30 12:58:06
 * @LastEditTime: 2023-08-01 18:23:22
 * @Description: The MediaPipe Face Landmark Model performs a single-camera face landmark detection in the screen 
    coordinate space: the X- and Y- coordinates are normalized screen coordinates, while the Z coordinate is relative 
	and is scaled as the X coordinate under the weak perspective projection camera model. 
 * @FilePath: \Mediapipe-MNN\source\FaceLandmarkDetector.cc
 */
#include <cfloat>
#include <iostream>
#include <opencv2/imgproc.hpp>
#include "util/Utils.h"
#include "CommonData.h"
#include "FaceLandmarkDetector.h"

using namespace MNN;
namespace dms
{
	FaceLandmarkDetector::~FaceLandmarkDetector() {
		net_->releaseModel();
		net_->releaseSession(session_);
	}

	bool FaceLandmarkDetector::loadModel(const char* model_file)
	{
		// 1. Load model
		net_ = std::unique_ptr<Interpreter>(Interpreter::createFromFile(model_file));
		if (net_ == nullptr) {
			std::cout << "Failed load model." << std::endl;
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

		CVLOGD("Face landmark detection model loaded, input size:(%d, %d, %d).\n", input_c_, input_h_, input_w_);
		inited_ = true;
		return true;
	}

	void FaceLandmarkDetector::setFormat(int source_format) {
		// Create image process
		CV::ImageProcess::Config image_process_config;
		image_process_config.filterType = CV::BILINEAR;
		image_process_config.sourceFormat = CV::ImageFormat(source_format);
		image_process_config.destFormat = CV::RGB;
		image_process_config.wrap = CV::ZERO;
		memcpy(image_process_config.mean, meanVals_, sizeof(meanVals_));
		memcpy(image_process_config.normal, normVals_, sizeof(normVals_));
		pretreat_ = std::shared_ptr<CV::ImageProcess>(
			CV::ImageProcess::create(image_process_config));
	}

	bool FaceLandmarkDetector::detect(const ImageHead& in, RotateType type, std::vector<ObjectInfo>& objects)
	{
		if (in.data == nullptr) {
			CVLOGE("Input image is empty.");
			return false;
		}

		if (!inited_) {
			CVLOGE("Face landmark detection model uninited.");
			return false;
		}

		int width = in.width;
		int height = in.height;
		for (auto& object : objects)
		{
			std::vector<Point2f> region = getInputRegion(in, type, object.rect, input_h_, input_w_, object.angle, 1.5f, 0.0f, 0.0f);
			
			// clang-format off
			float points_src[] = {							  // 原图坐标
			  region[0].x, region[0].y,						  // 0------2
			  region[1].x, region[1].y,						  // |	    |
			  region[2].x, region[2].y,						  // |	    |
			  region[3].x, region[3].y,						  // 1------3
			};												  
			float points_dst[] = {							  // 输入坐标
			  0.0f,  0.0f,									  // 0------2
			  0.0f, (float)(input_h_ - 1),					  // |	    |
			  (float)(input_w_ - 1), 0.0f,					  // |	    |
			  (float)(input_w_ - 1), (float)(input_h_ - 1),	  // 1------3
			};
			
			// clang-format on
			trans_.setPolyToPoly((CV::Point*)points_dst, (CV::Point*)points_src, 4);
			pretreat_->setMatrix(trans_);
			pretreat_->convert((uint8_t*)in.data, width, height, in.width_step, input_tensor_);

			// Inference
			int ret = net_->runSession(session_);
			if (ret != 0) {
				CVLOGE("Failed to inference.");
				return -1;
			}

			// Get the result
			MNN::Tensor* face_mesh = net_->getSessionOutput(session_, "Identity");
			MNN::Tensor* face_score = net_->getSessionOutput(session_, "Identity_1");
			//MNN::Tensor *tongue_score = net_->getSessionOutput(sess_, "Identity_2");

			std::shared_ptr<MNN::Tensor> output_face_mesh(
				new MNN::Tensor(face_mesh, face_mesh->getDimensionType()));
			std::shared_ptr<MNN::Tensor> output_face_score(
				new MNN::Tensor(face_score, face_score->getDimensionType()));
			//std::shared_ptr<MNN::Tensor> output_tongue_score(
			//   new MNN::Tensor(tongue_score, tongue_score->getDimensionType()));

			face_mesh->copyToHostTensor(output_face_mesh.get());
			face_score->copyToHostTensor(output_face_score.get());
			//tongue_score->copyToHostTensor(output_tongue_score.get());

			Point3f landmark;
			object.landmarks_3D.resize(kNumFaceLandmarks);
			float* mesh_ptr = output_face_mesh->host<float>();
			for (int k = 0; k < kNumFaceLandmarks; ++k)
			{
				landmark.x = mesh_ptr[3 * k + 0];
				landmark.y = mesh_ptr[3 * k + 1];
				landmark.z = mesh_ptr[3 * k + 2]; // 相机光轴方向为Z正方向
				object.landmarks_3D[k].x = trans_[0] * landmark.x + trans_[1] * landmark.y + trans_[2];
				object.landmarks_3D[k].y = trans_[3] * landmark.x + trans_[4] * landmark.y + trans_[5];
				object.landmarks_3D[k].z = landmark.z;
			}

			float* face_score_ptr = output_face_score->host<float>();
			object.score = sigmoid(face_score_ptr[0]);
		}

		return true;
	}

	void FaceLandmarkDetector::drawFaceMesh(cv::Mat& src, const std::vector<Point3f>& landmarks_3D)
	{
		if (src.empty()) {
			CVLOGE("Source image is empty, %s", __FILE__);
			return;
		}

		int line_num = sizeof(FACEMESH_TESSELATION) / sizeof(ushort) / 2;
		for (int i = 0; i < line_num; ++i)
		{
			ushort idx1 = FACEMESH_TESSELATION[i][0];
			ushort idx2 = FACEMESH_TESSELATION[i][1];
			cv::Point pt1(landmarks_3D[idx1].x, landmarks_3D[idx1].y);
			cv::Point pt2(landmarks_3D[idx2].x, landmarks_3D[idx2].y);
			cv::line(src, pt1, pt2, cv::Scalar(255, 188, 32));
		}
	}

	void FaceLandmarkDetector::drawLeftEyeContour(cv::Mat& src, const std::vector<Point3f>& landmarks_3D)
	{
		if (src.empty()) {
			CVLOGE("Source image is empty, %s", __FILE__);
			return;
		}

		int left_eye_landmark_size = landmark_group_.eye_left.size();
		for (int i = 0; i < left_eye_landmark_size; ++i)
		{
			ushort idx1 = landmark_group_.eye_left[i];
			ushort idx2 = landmark_group_.eye_left[(i + 1) % left_eye_landmark_size];
			cv::Point pt1(landmarks_3D[idx1].x, landmarks_3D[idx1].y);
			cv::Point pt2(landmarks_3D[idx2].x, landmarks_3D[idx2].y);
			cv::line(src, pt1, pt2, cv::Scalar(0, 0, 255));
		}
	}

	void FaceLandmarkDetector::drawRightEyeContour(cv::Mat& src, const std::vector<Point3f>& landmarks_3D)
	{
		if (src.empty()) {
			CVLOGE("Source image is empty, %s", __FILE__);
			return;
		}

		int right_eye_landmark_size = landmark_group_.eye_right.size();
		for (int i = 0; i < right_eye_landmark_size; ++i)
		{
			ushort idx1 = landmark_group_.eye_right[i];
			ushort idx2 = landmark_group_.eye_right[(i + 1) % right_eye_landmark_size];
			cv::Point pt1(landmarks_3D[idx1].x, landmarks_3D[idx1].y);
			cv::Point pt2(landmarks_3D[idx2].x, landmarks_3D[idx2].y);
			cv::line(src, pt1, pt2, cv::Scalar(0, 0, 255));
		}
	}

	void FaceLandmarkDetector::drawMouthContour(cv::Mat& src, const std::vector<Point3f>& landmarks_3D)
	{
		if (src.empty()) {
			CVLOGE("Source image is empty, %s", __FILE__);
			return;
		}

		int mouth_landmark_size = landmark_group_.mouth.size();
		for (int i = 0; i < mouth_landmark_size; ++i)
		{
			ushort idx1 = landmark_group_.mouth[i];
			ushort idx2 = landmark_group_.mouth[(i + 1) % mouth_landmark_size];
			cv::Point pt1(landmarks_3D[idx1].x, landmarks_3D[idx1].y);
			cv::Point pt2(landmarks_3D[idx2].x, landmarks_3D[idx2].y);
			cv::line(src, pt1, pt2, cv::Scalar(0, 255, 255));
		}
	}
} // namespace mirror
