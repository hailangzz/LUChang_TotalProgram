#include "DMSCore.h"
#include "util/Utils.h"
#include "CommonData.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <iostream>

dms::DMSCore::DMSCore()
{
    left_eye_inner_corner_idx[0] = 362;
    left_eye_inner_corner_idx[1] = 263;
    right_eye_inner_corner_idx[0] = 33;
    right_eye_inner_corner_idx[1] = 133;

    left_eye_contour_.resize(landmark_group_.eye_left.size());
    right_eye_contour_.resize(landmark_group_.eye_right.size());

    camera_matrix_ = (cv::Mat_<double>(3, 3) <<
        400,   0, 640,
          0, 400, 360,
          0,   0,   1);
}

void dms::DMSCore::updateLandmarks3D(const std::vector<Point3f>& landmarks_3D)
{
    landmarks_3D_.clear();
    landmarks_3D_.resize(landmarks_3D.size());
    for (int i = 0; i < landmarks_3D.size(); ++i) {
        landmarks_3D_[i].x = landmarks_3D[i].x;
        landmarks_3D_[i].y = landmarks_3D[i].y;
        landmarks_3D_[i].z = landmarks_3D[i].z;
    }
}

void dms::DMSCore::updateYoloDetectorObject(std::vector<BoxInfo> yolo_object)
{
    yolo_object_.clear();
    yolo_object_.resize(yolo_object.size());
    for (int i = 0; i < yolo_object.size(); ++i) {
        yolo_object_[i].x1 = yolo_object[i].x1;
        yolo_object_[i].y1 = yolo_object[i].y1;
        yolo_object_[i].x2 = yolo_object[i].x2;
        yolo_object_[i].y2 = yolo_object[i].y2;
        yolo_object_[i].label = yolo_object[i].label;
        yolo_object_[i].id = yolo_object[i].id;
        yolo_object_[i].score = yolo_object[i].score;
    }
}


bool dms::DMSCore::detectEyeCloseEvent()
{
    PERCLOS_ = estimatePERCLOS();

    if (PERCLOS_ < config_.PERCLOS_thresh)
    {
        eye_close_event_.detected = false;
        eye_close_event_.timestamp = 0.0;
        eye_close_event_.duration = 0.0;
        return false;
    }
    else
    {
        if (!eye_close_event_.detected) {
            eye_close_event_.detected = true;
            eye_close_event_.timestamp = double(cv::getTickCount());
            return false;
        }
        else
        {
            eye_close_event_.detected = true;
            double current_time = double(cv::getTickCount());
            eye_close_event_.duration += (current_time - eye_close_event_.timestamp) / cv::getTickFrequency();
            eye_close_event_.timestamp = current_time;

            if (eye_close_event_.duration < config_.eye_close_duration_thresh) {
                return false;
            }
        }
    }

    return true;
}

bool dms::DMSCore::detectYawnEvent()
{
    mouth_opening_degree_ = estimateMouthOpeningDegree();

    if (mouth_opening_degree_ < config_.mouth_opening_thresh)
    {
        yawn_event_.detected = false;
        yawn_event_.timestamp = 0.0;
        yawn_event_.duration = 0.0;
        return false;
    }
    else
    {
        if (!yawn_event_.detected) {
            yawn_event_.detected = true;
            yawn_event_.timestamp = double(cv::getTickCount());
            return false;
        }
        else
        {
            yawn_event_.detected = true;
            double current_time = double(cv::getTickCount());
            yawn_event_.duration += (current_time - yawn_event_.timestamp) / cv::getTickFrequency();
            yawn_event_.timestamp = current_time;

            if (yawn_event_.duration < config_.yawn_duration_thresh) {
                return false;
            }
        }
    }

    return true;
}

bool dms::DMSCore::detectSmokingEvent(BoxInfo object) {
    if (object.score < config_.smoking_threshold)
    {
        smoking_event_.detected = false;
        smoking_event_.timestamp = 0.0;
        smoking_event_.duration = 0.0;
        return false;
    }
    else
    {
        if (!smoking_event_.detected) {
            smoking_event_.detected = true;
            smoking_event_.timestamp = double(cv::getTickCount());
            return false;
        }
        else
        {
            smoking_event_.detected = true;
            double current_time = double(cv::getTickCount());
            smoking_event_.duration += (current_time - smoking_event_.timestamp) / cv::getTickFrequency();
            smoking_event_.timestamp = current_time;

            if (smoking_event_.duration < config_.smoking_duration_thresh) {
                return false;
            }
        }
    }

    return true;
}

bool dms::DMSCore::detectCallingEvent(BoxInfo object) {
    if (object.score < config_.calling_threshold)
    {
        calling_event_.detected = false;
        calling_event_.timestamp = 0.0;
        calling_event_.duration = 0.0;
        return false;
    }
    else
    {
        if (!calling_event_.detected) {
            calling_event_.detected = true;
            calling_event_.timestamp = double(cv::getTickCount());
            return false;
        }
        else
        {
            calling_event_.detected = true;
            double current_time = double(cv::getTickCount());
            calling_event_.duration += (current_time - calling_event_.timestamp) / cv::getTickFrequency();
            calling_event_.timestamp = current_time;

            if (calling_event_.duration < config_.calling_duration_thresh) {
                return false;
            }
        }
    }

    return true;
}

bool dms::DMSCore::detectSunglassesEvent(BoxInfo object) {
    if (object.score < config_.sunglasses_threshold)
    {
        sunglasses_event_.detected = false;
        sunglasses_event_.timestamp = 0.0;
        sunglasses_event_.duration = 0.0;
        return false;
    }
    else
    {
        if (!sunglasses_event_.detected) {
            sunglasses_event_.detected = true;
            sunglasses_event_.timestamp = double(cv::getTickCount());
            return false;
        }
        else
        {
            sunglasses_event_.detected = true;
            double current_time = double(cv::getTickCount());
            sunglasses_event_.duration += (current_time - sunglasses_event_.timestamp) / cv::getTickFrequency();
            sunglasses_event_.timestamp = current_time;

            if (sunglasses_event_.duration < config_.sunglasses_duration_thresh) {
                return false;
            }
        }
    }

    return true;
}

dms::DMSCore::DistractedDrivingEventBool dms::DMSCore::detectDistractedDrivingEvent() {
    
    for (const auto& object : yolo_object_) {
        
        //std::cout<<"object.label:"<< object.label<<"object.score:"<< object.score << std::endl;
        // smoking
        if (object.label == 0) {
            distracted_driving_event_bool.smoking_event_flag = detectSmokingEvent(object);
        }
        if (object.label == 1) {
            distracted_driving_event_bool.sunglasses_event_flag = detectSunglassesEvent(object);
        }
        if (object.label == 2) {
            distracted_driving_event_bool.calling_event_flag = detectCallingEvent(object);
        }        
    }
    //printf("calling_event_flag: %s, \n", (distracted_driving_event.calling_event_flag) ? "calling_event_flag is true" : "calling_event_flag is false");

    return distracted_driving_event_bool;
}

// 估算驾驶员眼睛的闭合度
float dms::DMSCore::estimatePERCLOS()
{
    left_eye_inner_corners[0].x = landmarks_3D_[left_eye_inner_corner_idx[0]].x;
    left_eye_inner_corners[0].y = landmarks_3D_[left_eye_inner_corner_idx[0]].y;
    left_eye_inner_corners[1].x = landmarks_3D_[left_eye_inner_corner_idx[1]].x;
    left_eye_inner_corners[1].y = landmarks_3D_[left_eye_inner_corner_idx[1]].y;

    right_eye_inner_corners[0].x = landmarks_3D_[right_eye_inner_corner_idx[0]].x;
    right_eye_inner_corners[0].y = landmarks_3D_[right_eye_inner_corner_idx[0]].y;
    right_eye_inner_corners[1].x = landmarks_3D_[right_eye_inner_corner_idx[1]].x;
    right_eye_inner_corners[1].y = landmarks_3D_[right_eye_inner_corner_idx[1]].y;

    // 整个眼睛的面积（假设为圆形）
    cv::Point2f d1 = left_eye_inner_corners[0] - left_eye_inner_corners[1];
    cv::Point2f d2 = right_eye_inner_corners[0] - right_eye_inner_corners[1];
    float left_eye_full_size = CV_PI * (d1.x * d1.x + d1.y * d1.y) / 4;
    float right_eye_full_size = CV_PI * (d2.x * d2.x + d2.y * d2.y) / 4;

    for (int i = 0; i < left_eye_contour_.size(); ++i) {
        left_eye_contour_[i].x = landmarks_3D_[landmark_group_.eye_left[i]].x;
        left_eye_contour_[i].y = landmarks_3D_[landmark_group_.eye_left[i]].y;
    }

    for (int j = 0; j < right_eye_contour_.size(); ++j) {
        right_eye_contour_[j].x = landmarks_3D_[landmark_group_.eye_right[j]].x;
        right_eye_contour_[j].y = landmarks_3D_[landmark_group_.eye_right[j]].y;
    }

    // 未闭合区域面积
    float left_eye_open_size = cv::contourArea(left_eye_contour_);
    float right_eye_open_size = cv::contourArea(right_eye_contour_);
    
    // PERCLOS为左眼和右眼PERCLOS的最大值
    float PERCLOS_left = 1.f - (left_eye_open_size / left_eye_full_size);
    float PERCLOS_right = 1.f - (right_eye_open_size / right_eye_full_size);

    return MAX_(PERCLOS_left, PERCLOS_right);
}

float dms::DMSCore::estimateMouthOpeningDegree()
{
    // 左右嘴角关键点
    cv::Point2f l0(landmarks_3D_[78].x, landmarks_3D_[78].y);
    cv::Point2f l1(landmarks_3D_[308].x, landmarks_3D_[308].y);
    
    // 上下嘴唇关键点
    cv::Point2f h0(landmarks_3D_[82].x, landmarks_3D_[82].y);
    cv::Point2f h1(landmarks_3D_[87].x, landmarks_3D_[87].y);
    cv::Point2f h2(landmarks_3D_[13].x, landmarks_3D_[13].y);
    cv::Point2f h3(landmarks_3D_[14].x, landmarks_3D_[14].y);
    cv::Point2f h4(landmarks_3D_[312].x, landmarks_3D_[312].y);
    cv::Point2f h5(landmarks_3D_[317].x, landmarks_3D_[317].y);

    float mouth_openging_degree = (cv::norm(h0 - h1) + cv::norm(h2 - h3) + cv::norm(h4 - h5)) 
        / (cv::norm(l0 - l1) * 3);

    return mouth_openging_degree;
}

bool DMSCore::estimateHeadPose()
{
    head_pose_landmarks_2D_.clear();
    head_pose_landmarks_3D_.clear();
    head_pose_landmarks_2D_.resize(landmark_group_.head_pose_idx.size());
    head_pose_landmarks_3D_.resize(landmark_group_.head_pose_idx.size());
    
    for (int i = 0; i < landmark_group_.head_pose_idx.size(); ++i)
    {
        head_pose_landmarks_2D_[i].x = landmarks_3D_[i].x;
        head_pose_landmarks_2D_[i].y = landmarks_3D_[i].y;
        
        // CanonicalFaceModel的Y、Z轴方向调整为与landmarks_3D像素坐标系一致
        head_pose_landmarks_3D_[i].x = CanonicalFaceModel[landmark_group_.head_pose_idx[i]][0];
        head_pose_landmarks_3D_[i].y = -CanonicalFaceModel[landmark_group_.head_pose_idx[i]][1];
        head_pose_landmarks_3D_[i].z = -CanonicalFaceModel[landmark_group_.head_pose_idx[i]][2];
    }

    cv::Mat head_pose_rvec;
    cv::solvePnP(head_pose_landmarks_3D_, head_pose_landmarks_2D_, 
        camera_matrix_, cv::noArray(), head_pose_rvec, head_pose_tvec_);
    cv::Rodrigues(head_pose_rvec, head_pose_rmat_);

    cv::Vec3f angles = eulerAngles(head_pose_rmat_);
    //printf("pitch: %.1f, yaw: %.1f, roll: %.1f\n", angles[0], angles[1], angles[2]);

    return false;
}

void dms::DMSCore::drawEyeCloseTips(cv::Mat& src, bool detected)
{
    char label[30];
    sprintf(label, "Eye closed: %.1fs", eye_close_event_.duration);
    cv::putText(src, label, cv::Point2f(20, 50), 1, 1.5, 
        detected ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0), 2);
}

void dms::DMSCore::drawYawnTips(cv::Mat& src, bool detected)
{
    char label[30];
    sprintf(label, "Yawn: %.1fs", yawn_event_.duration);
    cv::putText(src, label, cv::Point2f(20, 80), 1, 1.5,
        detected ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0), 2);
}

void dms::DMSCore::drawDistractedDrivingTips(cv::Mat& src, DistractedDrivingEventBool detected) {
    char label[30];
    sprintf(label, "calling : %.1fs", calling_event_.duration);
    cv::putText(src, label, cv::Point2f(20, 110), 1, 1.5,
        detected.calling_event_flag ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0), 2);

    
    sprintf(label, "smoking : %.1fs", smoking_event_.duration);
    cv::putText(src, label, cv::Point2f(20, 140), 1, 1.5,
        detected.smoking_event_flag ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0), 2);

    
    sprintf(label, "sunglasses : %.1fs", sunglasses_event_.duration);
    cv::putText(src, label, cv::Point2f(20, 170), 1, 1.5,
        detected.sunglasses_event_flag ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0), 2);
}
