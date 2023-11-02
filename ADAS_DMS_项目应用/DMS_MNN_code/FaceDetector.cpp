#include "FaceDetector.h"
#include "CommonData.h"
#include "util/Utils.h"
#include <iostream>

using namespace MNN;

namespace dms {
	FaceDetector::~FaceDetector() {
		net_->releaseModel();
		net_->releaseSession(session_);
	}

	bool FaceDetector::loadModel(const char* model_file)
	{
		// 1. Load model
		net_ = std::unique_ptr<Interpreter>(Interpreter::createFromFile(model_file));
		if (net_ == nullptr) {
			CVLOGE("Failed to load face detection model.");
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

		cls_name_ = "classificators";
		reg_name_ = "regressors";

		CVLOGD("Face detection model loaded, input size:(%d, %d, %d).\n", input_c_, input_h_, input_w_);
		inited_ = true;
		return true;
	}

	void FaceDetector::setFormat(int source_format)
	{
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

	void FaceDetector::setUseFull() {
		use_full_ = true;
		cls_name_ = "Identity_1";
		reg_name_ = "Identity";
	}

	bool FaceDetector::detect(const ImageHead& input, RotateType type, std::vector<ObjectInfo>& objects)
	{
		objects.clear();
		if (input.data == nullptr) {
			CVLOGE("Input image is empty.");
			return false;
		}

		if (!inited_) {
			CVLOGE("Face detection model uninited.");
			return false;
		}

		// 1. Set input
		int width = input.width;
		int height = input.height;
		std::vector<Point2f> input_region = getInputRegion(input, type, input_w_, input_h_);
		float points_src[] = {
		  input_region[0].x, input_region[0].y,
		  input_region[1].x, input_region[1].y,
		  input_region[2].x, input_region[2].y,
		  input_region[3].x, input_region[3].y,
		};

		float points_dst[] = {
		  0.0f, 0.0f,
		  0.0f, (float)(input_h_ - 1),
		  (float)(input_w_ - 1), 0.0f,
		  (float)(input_w_ - 1), (float)(input_h_ - 1)
		};
		trans_.setPolyToPoly((CV::Point*)points_dst, (CV::Point*)points_src, 4);
		pretreat_->setMatrix(trans_);
		pretreat_->convert((uint8_t*)input.data, width, height, input.width_step, input_tensor_);

		// 2. Inference
		int ret = net_->runSession(session_);
		if (ret != 0) {
			CVLOGE("Failed to inference.");
			return -1;
		}

		// 3. Get the result
		MNN::Tensor* classifier = net_->getSessionOutput(session_, cls_name_.c_str());
		MNN::Tensor* regressor = net_->getSessionOutput(session_, reg_name_.c_str());
		if (nullptr == classifier || nullptr == regressor) {
			CVLOGE("Error output.");
			return false;
		}
		std::shared_ptr<MNN::Tensor> output_classifier(new MNN::Tensor(classifier, classifier->getDimensionType()));
		std::shared_ptr<MNN::Tensor> output_regressor(new MNN::Tensor(regressor, regressor->getDimensionType()));
		classifier->copyToHostTensor(output_classifier.get());
		regressor->copyToHostTensor(output_regressor.get());

		// 4. Parse the result
		parseOutputs(output_classifier.get(), output_regressor.get(), objects);
		NMSObjects(objects, iou_thresh_);

		return true;
	}

	void FaceDetector::parseOutputs(MNN::Tensor* scores, MNN::Tensor* boxes,
		std::vector<ObjectInfo>& objects) {
		objects.clear();
		float* scores_ptr = scores->host<float>();
		float* boxes_ptr = boxes->host<float>();

		int batch = scores->batch();     // N
		int channel = scores->channel(); // C
		int height = scores->height();   // H
		int width = scores->width();     // W

 		float* anchors = BLAZE_FACE_ANCHORS;
		if (use_full_) {
			anchors = FULL_BLAZE_FACE_ANCHORS;
		}

		ObjectInfo object;
		// 2 x 16 x 16 (2:anchors, 16x16:feature map)
		// 6 x  8 x  8 (6:anchors,  8x8:feature map)
		for (int i = 0; i < channel; ++i)
		{
			object.score = sigmoid(scores_ptr[i]);
			if (object.score < score_thresh_)
				continue;

			float anchor_cx = anchors[4 * i + 0] * input_w_;
			float anchor_cy = anchors[4 * i + 1] * input_h_;
			
			// x,y,w,h: 4, landmarks: 6*2
			float* ptr = boxes_ptr + 16 * i;
			float cx = ptr[0] + anchor_cx;
			float cy = ptr[1] + anchor_cy;
			float w  = ptr[2];
			float h  = ptr[3];

			Point2f tl, br, tl_origin, br_origin;
			tl.x = cx - 0.5f * w;
			tl.y = cy - 0.5f * h;
			br.x = cx + 0.5f * w;
			br.y = cy + 0.5f * h;

			Point2f pt;
			object.landmarks_2D.resize(6);
			for (int k = 0; k < 6; ++k) {
				pt.x = ptr[4 + 2 * k + 0] + anchor_cx;
				pt.y = ptr[4 + 2 * k + 1] + anchor_cy;
				object.landmarks_2D[k].x = trans_[0] * pt.x + trans_[1] * pt.y + trans_[2];
				object.landmarks_2D[k].y = trans_[3] * pt.x + trans_[4] * pt.y + trans_[5];
			}

			// Parse the index landmarks
			Point2f src, dst;
			src.x = ptr[4] + anchor_cx;
			src.y = ptr[5] + anchor_cy;
			dst.x = ptr[6] + anchor_cx;
			dst.y = ptr[7] + anchor_cy;

			float dx = dst.x - src.x;
			float dy = dst.y - src.y;
			object.angle = -(kTargetFaceAngle - std::atan2(-dy, dx) * 180.0f / M_PI); 

			tl_origin.x = trans_[0] * tl.x + trans_[1] * tl.y + trans_[2];
			tl_origin.y = trans_[3] * tl.x + trans_[4] * tl.y + trans_[5];
			br_origin.x = trans_[0] * br.x + trans_[1] * br.y + trans_[2];
			br_origin.y = trans_[3] * br.x + trans_[4] * br.y + trans_[5];

			object.rect.left = MIN_(tl_origin.x, br_origin.x);
			object.rect.top = MIN_(tl_origin.y, br_origin.y);
			object.rect.right = MAX_(tl_origin.x, br_origin.x);
			object.rect.bottom = MAX_(tl_origin.y, br_origin.y);
			objects.emplace_back(object);
		}
	}

} // namespace mirror
