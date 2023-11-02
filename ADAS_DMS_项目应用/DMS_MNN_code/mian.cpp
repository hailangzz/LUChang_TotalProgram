#include "DMSCore.h"
#include "TypeDefines.h"
#include "FaceDetector.h"
#include "FaceLandmarkDetector.h"
#include "YoloDetector.h"
#include <cmath>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>
#include <util/Utils.h>

using namespace std;
using namespace dms;

#define USE_VIDEO

const string video_path = "../data/video/035_19700101083658.mp4";   // Wu Shuanglong
//const string video_path = "../data/video/069_20230919164615.mp4"; // Huang Jian
//const string video_path = "../data/video/dms_test.mp4";		      // RGB 1
//const string video_path = "../data/video/11-MaleGlasses-Yawning.avi"; // RGB 2

int main()
{
	// Turn off unnecessary logs in opencv
	cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);

	const char* image_path = "../data/images/calling.jpg";
	const char* face_detection_model_path = "../data/models/face_detection_short_range_fp16.mnn";
	const char* face_landmarks_model_path = "../data/models/face_landmarks_detector_fp16.mnn";
	const char* yolo_detection_model_path = "../data/models/distracted_driving_detection.mnn";

	FaceDetector detector;
	FaceLandmarkDetector landmarker;
	YoloDetector yolodetector;

	DMSCore dms_core;
	if (!detector.loadModel(face_detection_model_path)) {
		std::cout << "Failed to load face detection model." << std::endl;
		return -1;
	}

	if (!landmarker.loadModel(face_landmarks_model_path)) {
		std::cout << "Failed to load face landmarks model." << std::endl;
		return -1;
	}
	if (!yolodetector.loadModel(yolo_detection_model_path)) {
		std::cout << "Failed to load yolo detection model." << std::endl;
		return -1;
	}

	detector.setFormat(PixelFormat::BGR);
	landmarker.setFormat(PixelFormat::BGR);

	cv::Mat frame;

#ifdef USE_VIDEO
	cv::VideoCapture cap(video_path);
	if (!cap.isOpened()) {
		CVLOGE("Failed open camera.\n");
		return -2;

	}

	while (cv::waitKey(1) != 'q')
	{
		cap.read(frame);
		if (frame.empty()) {
			printf("Empty frame.\n");
			return -3;
		}
#else
	frame = cv::imread(image_path, 1);
#endif

	cv::resize(frame, frame, cv::Size(1280, 720));
	cv::Mat frame_src = frame.clone();

	RotateType type = RotateType::CLOCKWISE_ROTATE_0;
	if (type == CLOCKWISE_ROTATE_90) {
		cv::transpose(frame, frame);
	}
	else if (type == CLOCKWISE_ROTATE_180) {
		cv::flip(frame, frame, -1);
	}
	if (type == CLOCKWISE_ROTATE_270) {
		cv::transpose(frame, frame);
		cv::flip(frame, frame, 1);
	}

	ImageHead in;
	in.data = frame.data;
	in.height = frame.rows;
	in.width = frame.cols;
	in.width_step = frame.step[0];
	in.pixel_format = PixelFormat::BGR;

	static util::AvgTimeMeasure timer("Detect", 10);
	timer.begin();

	std::vector<ObjectInfo> objects;
	std::vector<BoxInfo> yolo_object;

	detector.detect(in, type, objects);
	landmarker.detect(in, type, objects);
	yolodetector.detect(frame_src, yolo_object);

	//for (const auto& box_info : yolo_object) {
	//		std::cout << "box_info.label" << box_info.label << "box_info.score" << box_info.score << std::endl;
	//	}

	timer.end();

	for (const auto& object : objects)
	{
		// Face detection bounding box and landmarks
		cv::rectangle(frame_src, cv::Point2f(object.rect.left, object.rect.top),
			cv::Point2f(object.rect.right, object.rect.bottom),
			cv::Scalar(255, 0, 255), 2);

		cv::putText(frame_src, std::to_string(object.score),
			cv::Point2f(object.rect.left, object.rect.top + 20), 1, 1.0,
			cv::Scalar(0, 255, 255));

		

		//for (int i = 0; i < object.landmarks_2D.size(); ++i) {
		//	cv::Point pt = cv::Point((int)object.landmarks_2D[i].x, (int)object.landmarks_2D[i].y);
		//	cv::circle(frame_src, pt, 2, cv::Scalar(255, 255, 0));
		//	cv::putText(frame_src, std::to_string(i), pt, 1, 1.0, cv::Scalar(255, 0, 255));
		//}

		// Draw Facemesh
		landmarker.drawFaceMesh(frame_src, object.landmarks_3D);
		landmarker.drawLeftEyeContour(frame_src, object.landmarks_3D);
		landmarker.drawRightEyeContour(frame_src, object.landmarks_3D);
		landmarker.drawMouthContour(frame_src, object.landmarks_3D);

		dms_core.updateLandmarks3D(object.landmarks_3D);		
		bool eye_close_detected = dms_core.detectEyeCloseEvent();
		bool yawn_detected = dms_core.detectYawnEvent();
		dms_core.drawEyeCloseTips(frame_src, eye_close_detected);
		dms_core.drawYawnTips(frame_src, yawn_detected);

	}

	dms_core.updateYoloDetectorObject(yolo_object);	
	auto distracted_driving_event_bool = dms_core.detectDistractedDrivingEvent();
	//distracted_driving_event_bool = dms_core.
	dms_core.drawDistractedDrivingTips(frame_src, distracted_driving_event_bool);

#ifdef USE_VIDEO
	cv::imshow("frame", frame_src);
	int key = cv::waitKey(2);
	if (key == 27) 
		break;
}
cap.release();
#else
	cv::imshow("Face landmarks", frame_src);
	cv::waitKey(0);
	cv::imwrite("../data/results/mediapipe_face.jpg", frame_src);
#endif
	return 0;
}