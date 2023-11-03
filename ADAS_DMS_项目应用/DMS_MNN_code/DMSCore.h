#pragma once

#include "FaceLandmarkDetector.h"

using namespace dms;

namespace dms
{
	class DMSCore
	{
		struct Config // 报警策略相关参数
		{
			float PERCLOS_thresh = 0.9;
			float mouth_opening_thresh = 0.6;
			float eye_close_duration_thresh = 2.f;		    // 单位：秒
			float yawn_duration_thresh = 3.f;			    // 单位：秒
			float abnormal_head_pose_duration_thresh = 3.f; // 单位：秒

			float calling_threshold = 0.3;			        // 置信度
			float calling_duration_thresh = 1.5f;			// 单位：秒
			float smoking_threshold = 0.3;			        // 置信度
			float smoking_duration_thresh = 1.5f;		    // 单位：秒
			float sunglasses_threshold = 0.3;			    // 置信度
			float sunglasses_duration_thresh = 1.5f;		    // 单位：秒
		};

		struct DistractedDrivingEventBool {
			bool calling_event_flag = false;                //打电话事件状态
			bool smoking_event_flag = false;				//抽烟事件状态		
			bool sunglasses_event_flag = false;				//戴墨镜事件状态
		};

		struct Event
		{
			bool detected = false; // 是否检测到某事件
			double timestamp;      // 检测到事件的时间
			double duration = 0;   // 事件累计时长(秒)
		};

	public:
		DMSCore();

		void updateLandmarks3D(const std::vector<Point3f>& landmarks_3D);	
		void updateYoloDetectorObject(std::vector<BoxInfo> yolo_object);

		bool detectEyeCloseEvent();
		bool detectYawnEvent();

		bool detectCallingEvent(BoxInfo object);
		bool detectSmokingEvent(BoxInfo object);
		bool detectSunglassesEvent(BoxInfo object);
		DistractedDrivingEventBool detectDistractedDrivingEvent(cv::Mat& src);
		

		
		float estimatePERCLOS();
		float estimateMouthOpeningDegree();
		bool  estimateHeadPose();

		void drawEyeCloseTips(cv::Mat& src, bool detected);
		void drawYawnTips(cv::Mat& src, bool detected);
		void drawDistractedDrivingTips(cv::Mat& src, DistractedDrivingEventBool detected);

	private:
		Config config_;
		float PERCLOS_;	                             // Percentage of eyelid closure over the pupil over time, PERCLOS
		float mouth_opening_degree_;
		ushort left_eye_inner_corner_idx[2];
		ushort right_eye_inner_corner_idx[2];
		cv::Point2f left_eye_inner_corners[2];
		cv::Point2f right_eye_inner_corners[2];
		std::vector<cv::Point2f> left_eye_contour_;
		std::vector<cv::Point2f> right_eye_contour_;

		Event eye_close_event_;
		Event yawn_event_;

		Event calling_event_;
		Event smoking_event_;
		Event sunglasses_event_;
		DistractedDrivingEventBool distracted_driving_event_bool;

		int lost_target_detection_maxnum_ = 10;      // 目标检测最大丢失帧数
		int now_lost_target_detection_num_ = 0;      // 目标检测当前丢失帧数


		cv::Mat head_pose_rmat_;
		cv::Mat head_pose_tvec_;
		cv::Mat camera_matrix_;
		LandmarkGroup landmark_group_;
		std::vector<cv::Point3f> landmarks_3D_;
		std::vector<cv::Point2f> head_pose_landmarks_2D_;
		std::vector<cv::Point3f> head_pose_landmarks_3D_;
		std::vector<BoxInfo> yolo_object_;           // 分心驾驶目标检测数组
	};
}